// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const ItemMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ItemMixinInterface> => {
      class ItemMixin extends superClass {
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
      }

      return ItemMixin;
    });

export interface ItemMixinInterface {
  appOrExtension(
      type: chrome.developerPrivate.ExtensionType, appLabel: string,
      extensionLabel: string): string;
  a11yAssociation(name: string): string;
}
