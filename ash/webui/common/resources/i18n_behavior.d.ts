// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SanitizeInnerHtmlOpts} from './parse_html_subset.js';

export interface I18nBehavior {
  locale: string|null|undefined;
  i18nUpdateLocale(): void;
  i18n(id: string, ...varArgs: Array<string|number>): string;
  i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts): string;
  i18nDynamic(locale: string, id: string, ...varArgs: string[]): string;
  i18nRecursive(locale: string, id: string, ...varArgs: string[]): string;
  i18nExists(id: string): boolean;
}

declare const I18nBehavior: object;
