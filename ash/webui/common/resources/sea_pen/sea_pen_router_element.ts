// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';
import './sea_pen_freeform_element.js';
import './sea_pen_images_element.js';
import './sea_pen_input_query_element.js';
import './sea_pen_introduction_dialog_element.js';
import './sea_pen_recent_wallpapers_element.js';
import './sea_pen_samples_element.js';
import './sea_pen_template_query_element.js';
import './sea_pen_templates_element.js';
import './sea_pen_toast_element.js';

import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {QUERY, Query} from './constants.js';
import {isSeaPenEnabled, isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {cleanUpSeaPenQueryStates, closeSeaPenIntroductionDialog, getShouldShowSeaPenIntroductionDialog} from './sea_pen_controller.js';
import {SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {logSeaPenVisited} from './sea_pen_metrics_logger.js';
import {SeaPenObserver} from './sea_pen_observer.js';
import {getTemplate} from './sea_pen_router_element.html.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {SeaPenTemplateQueryElement} from './sea_pen_template_query_element.js';
import {getTemplateIdFromString} from './sea_pen_utils.js';
import {maybeDoPageTransition} from './transition.js';

export enum SeaPenPaths {
  TEMPLATES = '',
  RESULTS = '/results',
  FREEFORM = '/freeform',
}

export interface SeaPenQueryParams {
  seaPenTemplateId?: string;
}

let instance: SeaPenRouterElement|null = null;

export class SeaPenRouterElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-router';
  }
  static get template() {
    return getTemplate();
  }
  static get properties() {
    return {
      basePath: String,

      path_: String,

      query_: String,

      queryParams_: Object,

      relativePath_: {
        type: String,
        computed: 'computeRelativePath_(path_, basePath)',
        observer: 'onRelativePathChanged_',
      },

      showSeaPenIntroductionDialog_: Boolean,
    };
  }

  static instance(): SeaPenRouterElement {
    assert(instance, 'sea pen router does not exist');
    return instance;
  }

  basePath: string;
  private path_: string;
  private query_: string;
  private queryParams_: SeaPenQueryParams;
  private relativePath_: string|null;
  private showSeaPenIntroductionDialog_: boolean;

  override connectedCallback() {
    assert(isSeaPenEnabled(), 'sea pen must be enabled');
    super.connectedCallback();
    instance = this;
    this.watch<SeaPenRouterElement['showSeaPenIntroductionDialog_']>(
        'showSeaPenIntroductionDialog_',
        state => state.shouldShowSeaPenIntroductionDialog);
    this.updateFromStore();
    this.fetchIntroductionDialogStatus();
    logSeaPenVisited(this.relativePath_ as SeaPenPaths);
    afterNextRender(this, () => SeaPenObserver.initSeaPenObserverIfNeeded());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    instance = null;
  }

  selectSeaPenTemplate(templateId: SeaPenTemplateId|Query|undefined) {
    if (templateId === undefined) {
      return;
    }
    // Clean up the Sea Pen states such as thumbnail response status code,
    // thumbnail loading status and Sea Pen query when
    // switching template; otherwise, states from the last query search will
    // remain in sea-pen-images element.
    cleanUpSeaPenQueryStates(this.getStore());
    if (templateId === QUERY) {
      this.goToRoute(SeaPenPaths.FREEFORM);
      return;
    }
    this.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: templateId.toString()});
  }

  async goToRoute(
      relativePath: SeaPenPaths, queryParams: SeaPenQueryParams = {}) {
    assert(typeof this.basePath === 'string', 'basePath must be set');
    const routingPath = this.basePath + relativePath;
    // Skip page transition animation if no changes in routing path.
    if (this.path_ === routingPath) {
      this.setProperties({queryParams_: queryParams});
      return Promise.resolve();
    }
    return maybeDoPageTransition(
        () => this.setProperties(
            {path_: routingPath, queryParams_: queryParams}));
  }

  /**
   * Compute the relative path compared to the SeaPen base path.
   * @param path the absolute path of the current route
   * @param basePath the absolute path of the base seapen route
   * @returns path relative to basePath, or null if path is not relative to
   *     basePath
   * @example
   * computeRelativePath_('/wallpaper/sea_pen', '/wallpaper/sea_pen') => ''
   * computeRelativePath_('/wallpaper/sea_pen/results', '/wallpaper/sea_pen') =>
   *   '/results'
   * computeRelativePath_('/wallpaper', '/wallpaper/sea_pen') => null
   */
  private computeRelativePath_(path: string|null, basePath: string|null): string
      |null {
    if (typeof path !== 'string' || typeof basePath !== 'string') {
      return null;
    }
    if (!path.startsWith(basePath)) {
      return null;
    }
    const relativePath = path.substring(basePath.length);
    // Normalize single slash to empty string.
    // This keeps path consistent between chrome://vc-background/ and
    // chrome://personalization/wallpaper/sea-pen.
    return relativePath === '/' ? '' : relativePath;
  }

  private onRelativePathChanged_(relativePath: string|null) {
    if (typeof relativePath !== 'string') {
      // `relativePath` will be null when using Personalization breadcrumbs to
      // navigate back to home or wallpaper. Don't reset the path, as
      // `SeaPenRouter` may be imminently torn down.
      return;
    }
    if (!Object.values(SeaPenPaths).includes(relativePath as SeaPenPaths)) {
      // If arriving at an unknown path, go back to the root path.
      console.warn('SeaPenRouter unknown path', relativePath);
      this.goToRoute(SeaPenPaths.TEMPLATES);
    }
  }

  private shouldShowTemplateQuery_(
      relativePath: string|null, templateId: string|null): boolean {
    return relativePath === SeaPenPaths.RESULTS &&
        (!!templateId && templateId !== 'Query');
  }

  private shouldShowSeaPenTemplates_(relativePath: string|null): boolean {
    if (typeof relativePath !== 'string') {
      return false;
    }
    return relativePath === SeaPenPaths.TEMPLATES;
  }

  private shouldShowSeaPenTemplateImages_(relativePath: string|null): boolean {
    if (typeof relativePath !== 'string') {
      return false;
    }
    return relativePath === SeaPenPaths.RESULTS;
  }

  private shouldShowSeaPenFreeform_(relativePath: string|null): boolean {
    return isSeaPenTextInputEnabled() && relativePath === SeaPenPaths.FREEFORM;
  }

  private onBottomContainerClicked_(): void {
    // close the chip option selection if it is open (or selected chip state is
    // set).
    this.shadowRoot!
        .querySelector<SeaPenTemplateQueryElement>('sea-pen-template-query')
        ?.onOptionSelectionDone();
  }

  private getTemplateIdFromQueryParams_(templateId: string): SeaPenTemplateId
      |Query {
    return getTemplateIdFromString(templateId);
  }

  private async fetchIntroductionDialogStatus() {
    await getShouldShowSeaPenIntroductionDialog(
        getSeaPenProvider(), this.getStore());
  }

  private async onCloseSeaPenIntroductionDialog_() {
    await closeSeaPenIntroductionDialog(getSeaPenProvider(), this.getStore());
    // Freeform focus goes to the text input automatically.
    if (this.relativePath_ !== SeaPenPaths.FREEFORM) {
      this.focusOnFirstTemplate_();
    }
  }

  private onRecentTemplateImageDelete_() {
    // focus on the first template if the deleted recent image is the only image
    // or the last image of recent images list.
    this.focusOnFirstTemplate_();
  }

  private focusOnFirstTemplate_() {
    const seaPenTemplates =
        this.shadowRoot!.querySelector<HTMLElement>('sea-pen-templates');
    const firstTemplate =
        seaPenTemplates!.shadowRoot!.querySelector<HTMLElement>(
            '.sea-pen-template');
    window.scrollTo(0, 0);
    firstTemplate!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sea-pen-router': SeaPenRouterElement;
  }
}

customElements.define(SeaPenRouterElement.is, SeaPenRouterElement);
