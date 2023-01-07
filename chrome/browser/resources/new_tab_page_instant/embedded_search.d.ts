// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.embeddedSearch.newTabPage API.
 * Embedded Search API methods defined in
 * chrome/renderer/searchbox/searchbox_extension.cc:
 *  NewTabPageBindings::GetObjectTemplateBuilder()
 */

declare namespace chrome {
  export namespace embeddedSearch {
    export namespace newTabPage {
      export interface MostVisitedItemData {
        direction: 'rtl'|'ltr';
        title: string;
        url: string;
      }
      export function getMostVisitedItemData(rid: number): MostVisitedItemData;

      export let ntpTheme: {
        alternateLogo: boolean,
        textColorLightRgba: number[],
        textColorRgba: number[],
        usingDefaultTheme: boolean,
        attribution1?: string,
        attribution2?: string,
        attributionActionUrl?: string,
        attributionUrl?: string,
        backgroundColorRgba?: number[],
        collectionId?: string, customBackgroundConfigured: boolean,
        imageHorizontalAlignment?: string,
        imageTiling?: string,
        imageUrl?: string,
        imageVerticalAlignment?: string,
      };
    }
  }
}
