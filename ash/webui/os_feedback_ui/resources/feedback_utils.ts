// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';

/**
 * @fileoverview
 * The file exports functions to handle scrolling.
 */

/** Change visual effect when the content is scrolling. */
export function showScrollingEffects(_event: Event, page: HTMLElement): void {
  const shadowShield =
      strictQuery('#shadowShield', page.shadowRoot, HTMLElement);
  const content = strictQuery('#content', page.shadowRoot, HTMLElement);
  const navButtons = strictQuery('#navButtons', page.shadowRoot, HTMLElement);
  const shadowElevation =
      strictQuery('#shadowElevation', page.shadowRoot, HTMLElement);
  const separator = strictQuery('#separator', page.shadowRoot, HTMLElement);

  // Debugging found the scrollTop reading sometimes was off by 0.61 px when the
  // content was scrolled to bottom. This caused the shadowShield be still
  // visible even though it should not be. Relax the condition to be not larger
  // than 1 px.
  const scrollingEnded: boolean =
      (content.scrollTop + content.clientHeight + 1 >= content.scrollHeight);

  shadowElevation.classList.toggle(
      'elevation-shadow-scrolling', content.scrollTop > 0);
  navButtons.classList.toggle('nav-buttons-scrolling', content.scrollTop > 0);
  shadowShield.classList.toggle('scrolling-shield', !scrollingEnded);
  separator.classList.toggle(
      'separator-scrolling-end', scrollingEnded && content.scrollTop > 0);
}

/**
 * Show initial effect only when the content is scrollable.
 */
export function showScrollingEffectOnStart(page: HTMLElement): void {
  const shadowShield =
      strictQuery('#shadowShield', page.shadowRoot, HTMLElement);
  const content = strictQuery('#content', page.shadowRoot, HTMLElement);
  shadowShield.classList.toggle(
      'scrolling-shield', content.scrollHeight > content.clientHeight);
}
