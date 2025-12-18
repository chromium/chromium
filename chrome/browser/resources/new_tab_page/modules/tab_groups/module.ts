// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/strings.m.js';
import '../info_dialog.js';
import '../module_header.js';
import './icons.html.js';
import './icon_container.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit} from '../../i18n_setup.js';
import {recordOccurrence, recordSmallCount} from '../../metrics_utils.js';
import {Color} from '../../tab_group_types.mojom-webui.js';
import type {PageHandlerRemote, TabGroup} from '../../tab_groups.mojom-webui.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import type {MenuItem} from '../module_header.js';

import {getCss} from './module.css.js';
import {getHtml} from './module.html.js';
import {TabGroupsProxyImpl} from './tab_groups_proxy.js';

export const MAX_TAB_GROUPS = 4;
export const COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX =
    '--color-new-tab-page-module-tab-groups-';
export const COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX =
    '--color-new-tab-page-module-tab-groups-dot-';

const ModuleElementBase = I18nMixinLit(CrLitElement);

export function colorIdToString(colorPrefix: string, id: Color): string {
  const colorMap = new Map<Color, string>([
    [Color.kGrey, 'grey'],
    [Color.kBlue, 'blue'],
    [Color.kRed, 'red'],
    [Color.kYellow, 'yellow'],
    [Color.kGreen, 'green'],
    [Color.kPink, 'pink'],
    [Color.kPurple, 'purple'],
    [Color.kCyan, 'cyan'],
    [Color.kOrange, 'orange'],
  ]);

  assert(colorMap.has(id));
  return colorPrefix + colorMap.get(id)!;
}

/**
 * The Tab Groups module, which helps users resume journey and discover tab
 * groups.
 */
export class ModuleElement extends ModuleElementBase {
  static get is() {
    return 'ntp-tab-groups';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaLabels: {type: Object},
      tabGroups: {type: Object},
      showInfoDialog: {type: Boolean},
    };
  }

  accessor ariaLabels: Map<string, string> = new Map();
  accessor tabGroups: TabGroup[] = [];
  accessor showInfoDialog: boolean = false;
  showZeroState: boolean = false;

  private handler_: PageHandlerRemote;

  constructor() {
    super();
    this.handler_ = TabGroupsProxyImpl.getInstance().handler;
  }

  override async updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('tabGroups')) {
      const entries = await Promise.all(this.tabGroups.map(async group => {
        const label = await this.computeTabGroupButtonAriaLabel_(group);
        return [group.id, label] as const;
      }));
      this.ariaLabels = new Map(entries);
    }
  }

  protected computeDescription_(time: string, device: string|null): string {
    return (device && device.length > 0) ? `${time} • ${device.trim()}` : time;
  }

  protected computeTabGroupColor_(color: Color): string {
    return colorIdToString(COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX, color);
  }

  protected computeTabGroupDotColor_(color: Color): string {
    return colorIdToString(
        COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX, color);
  }

  protected async computeTabGroupButtonAriaLabel_(group: TabGroup):
      Promise<string> {
    const totalTabsStr =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'modulesTabGroupsTabsText', group.totalTabCount);
    const description =
        this.computeDescription_(group.updateTime, group.deviceName);
    const sharedStr = group.isSharedTabGroup ? 'shared' : '';
    return [totalTabsStr, group.title, description, sharedStr]
        .filter(Boolean)
        .join(' ');
  }

  protected getMenuItems_(): MenuItem[] {
    return [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18nRecursive(
              '', 'modulesDismissForHoursButtonText',
              'tabGroupsModuleDismissHours'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesTabGroupsTitle'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
    ];
  }

  protected getTabGroups_(): TabGroup[] {
    return this.tabGroups.slice(0, MAX_TAB_GROUPS);
  }

  protected getFaviconUrls_(objects: Array<{url: string}>): string[] {
    return objects.map(obj => obj.url);
  }

  protected onDisableButtonClick_() {
    this.fire('disable-module', {
      message: this.i18n('modulesTabGroupsDisableToastMessage'),
    });
  }

  protected onDismissButtonClick_() {
    this.handler_.dismissModule();
    this.fire('dismiss-module-instance', {
      message: this.i18n('modulesTabGroupsDismissToastMessage'),
      restoreCallback: () => this.handler_.restoreModule(),
    });
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog = false;
  }

  protected onCreateNewTabGroupClick_(fromZeroStateCard: boolean) {
    this.fire('usage');
    const histogram = 'NewTabPage.TabGroups.CreateNewTabGroup';
    recordOccurrence(histogram);
    recordOccurrence(
        `${histogram}.${fromZeroStateCard ? 'ZeroState' : 'SteadyState'}`);

    this.handler_.createNewTabGroup();
  }

  protected onTabGroupClick_(id: string, index: number) {
    this.fire('usage');
    recordSmallCount('NewTabPage.TabGroups.ClickTabGroupIndex', index);
    this.handler_.openTabGroup(id);
  }
}

customElements.define(ModuleElement.is, ModuleElement);

async function createElement(): Promise<ModuleElement|null> {
  const {tabGroups, showZeroState} =
      await TabGroupsProxyImpl.getInstance().handler.getTabGroups();

  if (!tabGroups) {
    // Still within the dismissal time window--skip rendering module.
    return null;
  }

  if (!showZeroState && tabGroups.length === 0) {
    // If zero-state is disabled and there are no groups, skip rendering module.
    return null;
  }

  const element = new ModuleElement();
  element.tabGroups = tabGroups;
  element.showZeroState = showZeroState;
  return element;
}

export const tabGroupsDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'tab_groups', createElement);
