// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ItemDelegate} from './item.js';

type Constructor<T> = new (...args: any[]) => T;

export const ItemMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ItemMixinInterface> => {
      class ItemMixin extends superClass {
        static get properties() {
          return {
            /**
             * The underlying ExtensionInfo for the details being displayed.
             */
            data: Object,

            /* The item's delegate, or null. */
            delegate: Object,
          };
        }

        data: chrome.developerPrivate.ExtensionInfo;
        delegate: ItemDelegate;

        /**
         * Prevents reloading the same item while it's already being reloaded.
         */
        private isReloading_: boolean = false;

        /**
         * @return The app or extension label depending on |type|.
         */
        appOrExtension(
            type: chrome.developerPrivate.ExtensionType, appLabel: string,
            extensionLabel: string): string {
          const ExtensionType = chrome.developerPrivate.ExtensionType;
          switch (type) {
            case ExtensionType.HOSTED_APP:
            case ExtensionType.LEGACY_PACKAGED_APP:
            case ExtensionType.PLATFORM_APP:
              return appLabel;
            case ExtensionType.EXTENSION:
            case ExtensionType.SHARED_MODULE:
              return extensionLabel;
          }
          assertNotReached('Item type is not App or Extension.');
        }

        /**
         * @return The a11y association descriptor, e.g. "Related to <ext>".
         */
        a11yAssociation(name: string): string {
          // Don't use I18nMixin.i18n because of additional checks it
          // performs. Polymer ensures that this string is not stamped into
          // arbitrary HTML. `name` can contain any data including html tags,
          // e.g. "My <video> download extension!"
          return loadTimeData.getStringF('extensionA11yAssociation', name);
        }

        /**
         * Checks if this is an unpacked or disabled item that can be reloaded.
         *
         * @returns If the item can be reloaded.
         */
        canReloadItem(): boolean {
          // Only display the reload spinner if the extension is unpacked and
          // enabled or disabled for reload. If an extension fails to reload
          // (due to e.g. a parsing error), it will remain disabled with the
          // "reloading" reason. We show the reload button when it's disabled
          // for reload to enable developers to reload the fixed version. (Note
          // that trying to reload an extension that is currently trying to
          // reload is a no-op.) For other disableReasons, there's no point in
          // reloading a disabled extension, and we'll show a crashed reload
          // button if it's terminated.
          const showIcon = this.data.location ===
                  chrome.developerPrivate.Location.UNPACKED &&
              (this.data.state ===
                   chrome.developerPrivate.ExtensionState.ENABLED ||
               this.data.disableReasons.reloading);
          return showIcon;
        }

        /**
         * Reloads the item.
         */
        async reloadItem() {
          // Don't reload if in the middle of an update.
          if (this.isReloading_) {
            return;
          }

          this.isReloading_ = true;

          const toastManager = getToastManager();
          // Keep the toast open indefinitely.
          toastManager.duration = 0;
          toastManager.show(loadTimeData.getString('itemReloading'));

          try {
            await this.delegate.reloadItem(this.data.id);
            toastManager.hide();
            toastManager.duration = 3000;
            toastManager.show(loadTimeData.getString('itemReloaded'));
          } catch (loadError) {
            toastManager.hide();
            throw loadError;
          } finally {
            this.isReloading_ = false;
          }
        }
      }

      return ItemMixin;
    });

export interface ItemMixinInterface {
  appOrExtension(
      type: chrome.developerPrivate.ExtensionType, appLabel: string,
      extensionLabel: string): string;
  a11yAssociation(name: string): string;
  canReloadItem(): boolean;
  reloadItem(): Promise<void>;
}
