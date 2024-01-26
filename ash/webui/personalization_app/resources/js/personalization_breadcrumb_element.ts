// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {getSeaPenTemplates, SeaPenTemplate} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {isSeaPenEnabled} from 'chrome://resources/ash/common/sea_pen/load_time_booleans.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {GooglePhotosAlbum, TopicSource, WallpaperCollection} from '../personalization_app.mojom-webui.js';

import {getTemplate} from './personalization_breadcrumb_element.html.js';
import {isPathValid, Paths, PersonalizationRouterElement} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {inBetween} from './utils.js';
import {findAlbumById} from './wallpaper/utils.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    index: number,
  };
}

export function stringToTopicSource(x: string): TopicSource|null {
  const num = parseInt(x, 10);
  if (!isNaN(num) &&
      inBetween(num, TopicSource.MIN_VALUE, TopicSource.MAX_VALUE)) {
    return num;
  }
  return null;
}

export interface PersonalizationBreadcrumbElement {
  $: {
    container: HTMLElement,
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class PersonalizationBreadcrumbElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-breadcrumb';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /** The current Google Photos album id to display. */
      googlePhotosAlbumId: String,

      /** The topic source of the selected album(s) for screensaver. */
      topicSource: String,

      /** The current SeaPen template id to display. */
      seaPenTemplateId: String,

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      breadcrumbs_: {
        type: Array,
        computed:
            'computeBreadcrumbs_(path, collections_, collectionId, albums_, albumsShared_, googlePhotosAlbumId, seaPenTemplates_, seaPenTemplateId, topicSource)',
        observer: 'onBreadcrumbsChanged_',
      },

      collections_: {
        type: Array,
      },

      /** The list of Google Photos albums. */
      albums_: Array,

      /** The list of shared Google Photos albums. */
      albumsShared_: Array,

      /** The list of SeaPen templates. */
      seaPenTemplates_: {
        type: Array,
        computed: 'computeSeaPenTemplates_()',
      },

      /** The breadcrumb being highlighted by keyboard navigation. */
      selectedBreadcrumb_: {
        type: Object,
        notify: true,
      },
    };
  }

  collectionId: string;
  googlePhotosAlbumId: string;
  topicSource: string;
  seaPenTemplateId: string;
  path: string;
  private breadcrumbs_: string[];
  private collections_: WallpaperCollection[]|null;
  private albums_: GooglePhotosAlbum[]|null;
  private albumsShared_: GooglePhotosAlbum[]|null;
  private seaPenTemplates_: SeaPenTemplate[]|null;
  private selectedBreadcrumb_: HTMLElement;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.wallpaper.backdrop.collections);
    this.watch('albums_', state => state.wallpaper.googlePhotos.albums);
    this.watch(
        'albumsShared_', state => state.wallpaper.googlePhotos.albumsShared);
    this.updateFromStore();
  }

  private onBreadcrumbsChanged_() {
    requestAnimationFrame(() => {
      // Note that only 1 breadcrumb is focusable at any given time. When
      // breadcrumbs change, the previously selected breadcrumb might not be in
      // DOM anymore. To allow keyboard users to focus the breadcrumbs again, we
      // add the first breadcrumb back to tab order.
      const allBreadcrumbs = this.$.selector.items as HTMLElement[];
      const hasFocusableBreadcrumb =
          allBreadcrumbs.some(el => el.getAttribute('tabindex') === '0');

      if (!hasFocusableBreadcrumb && allBreadcrumbs.length > 0) {
        this.$.selector.selectIndex(0);
        allBreadcrumbs[0].setAttribute('tabindex', '0');
      }
    });
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevBreadcrumb = this.selectedBreadcrumb_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous breadcrumb.
    if (prevBreadcrumb) {
      prevBreadcrumb.removeAttribute('tabindex');
    }
    // Add focus state for new breadcrumb.
    if (this.selectedBreadcrumb_) {
      this.selectedBreadcrumb_.setAttribute('tabindex', '0');
      this.selectedBreadcrumb_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  /**
   * Returns the aria-current status of the breadcrumb. The last breadcrumb is
   * considered the "current" breadcrumb representing the active page.
   */
  private getBreadcrumbAriaCurrent_(index: number, breadcrumbs: string[]):
      'page'|'false' {
    if (index === (breadcrumbs.length - 1)) {
      return 'page';
    }
    return 'false';
  }

  private computeBreadcrumbs_(): string[] {
    const breadcrumbs = [];
    switch (this.path) {
      case Paths.COLLECTIONS:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        break;
      case Paths.COLLECTION_IMAGES:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        if (isNonEmptyArray(this.collections_)) {
          const collection = this.collections_.find(
              collection => collection.id === this.collectionId);
          if (collection) {
            breadcrumbs.push(collection.name);
          }
        }
        break;
      case Paths.GOOGLE_PHOTOS_COLLECTION:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('googlePhotosLabel'));
        const googlePhotosAlbum =
            findAlbumById(this.googlePhotosAlbumId, this.albums_) ??
            findAlbumById(this.googlePhotosAlbumId, this.albumsShared_);
        if (googlePhotosAlbum) {
          breadcrumbs.push(googlePhotosAlbum.title);
        } else if (this.googlePhotosAlbumId) {
          console.warn(
              'Can\'t find a matching album with id:',
              this.googlePhotosAlbumId);
        }
        break;
      case Paths.LOCAL_COLLECTION:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('myImagesLabel'));
        break;
      case Paths.SEA_PEN_COLLECTION:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('seaPenLabel'));
        break;
      case Paths.SEA_PEN_RESULTS:
        breadcrumbs.push(this.i18n('wallpaperLabel'));
        breadcrumbs.push(this.i18n('seaPenLabel'));
        if (this.seaPenTemplateId && isNonEmptyArray(this.seaPenTemplates_)) {
          const template = this.seaPenTemplates_.find(
              template => template.id.toString() === this.seaPenTemplateId);
          if (template) {
            breadcrumbs.push(template.title);
          }
        }
        break;
      case Paths.USER:
        breadcrumbs.push(this.i18n('avatarLabel'));
        break;
      case Paths.AMBIENT:
        breadcrumbs.push(this.i18n('screensaverLabel'));
        break;
      case Paths.AMBIENT_ALBUMS:
        breadcrumbs.push(this.i18n('screensaverLabel'));
        const topicSourceVal = stringToTopicSource(this.topicSource);
        if (topicSourceVal === TopicSource.kGooglePhotos) {
          breadcrumbs.push(this.i18n('ambientModeTopicSourceGooglePhotos'));
        } else if (topicSourceVal === TopicSource.kArtGallery) {
          breadcrumbs.push(this.i18n('ambientModeTopicSourceArtGallery'));
        } else if (topicSourceVal === TopicSource.kVideo) {
          breadcrumbs.push(this.i18n('ambientModeTopicSourceVideo'));
        } else {
          console.warn('Invalid TopicSource value.', topicSourceVal);
        }
        break;
    }
    return breadcrumbs;
  }

  private computeSeaPenTemplates_(): SeaPenTemplate[] {
    return getSeaPenTemplates();
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n('back', this.i18n('wallpaperLabel'));
  }

  private getHomeButtonAriaLabel_(): string {
    return this.i18n('ariaLabelHome');
  }

  private onBreadcrumbClick_(e: RepeaterEvent) {
    const index = e.model.index;
    // stay in same page if the user clicks on the last breadcrumb,
    // else navigate to the corresponding page.
    if (index < this.breadcrumbs_.length - 1) {
      const pathElements = this.path.split('/');
      const newPath = pathElements.slice(0, index + 2).join('/');
      if (isPathValid(newPath)) {
        // Unfocus the breadcrumb to focus on the page
        // with new path.
        const breadcrumb = e.target as HTMLElement;
        breadcrumb.blur();
        PersonalizationRouterElement.instance().goToRoute(newPath as Paths);
      }
    }
    // If the user clicks the last breadcrumb and the sea pen dropdown is
    // present, open the dropdown.
    const targetElement = e.currentTarget as HTMLElement;
    if (index === this.breadcrumbs_.length - 1 &&
        !!targetElement.querySelector('#seaPenDropdown')) {
      this.onClickMenuIcon_(e);
    }
  }

  private onClickMenuIcon_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const config = {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
    };
    const menuElement = this.shadowRoot!.querySelector('cr-action-menu');
    menuElement!.showAt(targetElement, config);
  }

  private onClickMenuItem_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const templateId = targetElement.dataset['id'];
    assert(!!templateId, 'templateId is required');
    PersonalizationRouterElement.instance().goToRoute(
        Paths.SEA_PEN_RESULTS, {seaPenTemplateId: templateId});
    this.closeOptionMenu_();
  }

  private closeOptionMenu_() {
    const menuElement = this.shadowRoot!.querySelector('cr-action-menu');
    menuElement!.close();
  }

  private shouldShowSeaPenDropdown_(path: string, breadcrumb: string): boolean {
    if (!isSeaPenEnabled()) {
      return false;
    }
    const template =
        this.seaPenTemplates_?.find(template => template.title === breadcrumb);

    return path === Paths.SEA_PEN_RESULTS && !!template;
  }

  private getAriaSelected_(
      templateId: SeaPenTemplateId, seaPenTemplateId: string): 'true'|'false' {
    return templateId.toString() === seaPenTemplateId ? 'true' : 'false';
  }

  private onHomeIconClick_() {
    PersonalizationRouterElement.instance().goToRoute(Paths.ROOT);
  }
}

customElements.define(
    PersonalizationBreadcrumbElement.is, PersonalizationBreadcrumbElement);
