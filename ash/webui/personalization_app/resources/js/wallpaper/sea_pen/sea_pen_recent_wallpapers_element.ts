// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the SeaPen recently used
 * wallpapers.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '../../../css/common.css.js';
import '../../../css/wallpaper.css.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {WithPersonalizationStore} from '../../personalization_store.js';
import {isNonEmptyArray} from '../../utils.js';
import {SeaPenWallpaper} from '../constants.js';
import {getRecentWallpaperImages} from '../wallpaper_controller.js';

import {getTemplate} from './sea_pen_recent_wallpapers_element.html.js';

export class SeaPenRecentWallpapersElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-recent-wallpapers';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      recentWallpapers_: Array,

      currentShowWallpaperInfoDialog_: {
        type: Number,
        value: null,
      },
    };
  }

  private recentWallpapers_: SeaPenWallpaper[]|null;
  private currentShowWallpaperInfoDialog_: number|null;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenRecentWallpapersElement['recentWallpapers_']>(
        'recentWallpapers_', state => state.wallpaper.seaPen.recentWallpapers);
    this.updateFromStore();
    // TODO(b/304576846): remove the function and use sea pen observer instead.
    getRecentWallpaperImages(this.getStore());
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private shouldShowRecentlyUsedWallpapers_(recentWallpapers: SeaPenWallpaper[]|
                                            null) {
    return isNonEmptyArray(recentWallpapers);
  }

  private isRecentWallpaperSelected_() {
    // TODO(b/307592600): set recent Sea Pen Wallpaper as wallpaper and update
    // timestamp.
  }

  private onClickMenuIcon_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const menuIconContainerRect = targetElement.getBoundingClientRect();
    const config = {
      top: menuIconContainerRect.top -
          8,  // 8px is the padding of .menu-icon-container
      left: menuIconContainerRect.left - menuIconContainerRect.width / 2,
      height: menuIconContainerRect.height,
      width: menuIconContainerRect.width,
      anchorAlignmentX: AnchorAlignment.AFTER_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_START,
    };
    const id = targetElement.dataset['id'];
    if (id !== undefined) {
      const index = parseInt(id, 10);
      const menuElement =
          this.shadowRoot!.querySelectorAll('cr-action-menu')![index];
      menuElement!.showAtPosition(config);
    }
  }

  private onClickMoreLikeThis_() {
    // TODO(b/304581483): make "More like this" button functional.
  }

  private onClickDeleteWallpaper_() {
    // TODO(b/304592162): delete the selected wallpaper.
  }

  private onClickWallpaperInfo_(e: Event) {
    const eventTarget = e.currentTarget as HTMLElement;
    const id = eventTarget.dataset['id'];
    if (id !== undefined) {
      this.currentShowWallpaperInfoDialog_ = parseInt(id, 10);
      this.closeAllActionMenus_();
    }
  }

  private closeAllActionMenus_() {
    const menuElements = this.shadowRoot!.querySelectorAll('cr-action-menu');
    menuElements.forEach(menuElement => {
      menuElement.close();
    });
  }

  private shouldShowWallpaperInfoDialog_(
      i: number, currentShowWallpaperInfoDialog: number|null): boolean {
    return currentShowWallpaperInfoDialog === i;
  }

  private onCloseDialog_() {
    this.currentShowWallpaperInfoDialog_ = null;
  }

  private getWallpaperInfoMessage_(image: SeaPenWallpaper): string {
    return `Image was generated using the text - ${image.query_info}`;
  }
}
customElements.define(
    SeaPenRecentWallpapersElement.is, SeaPenRecentWallpapersElement);
