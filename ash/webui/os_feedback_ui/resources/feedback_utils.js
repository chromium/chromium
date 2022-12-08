// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Change visual effect when the content is scrolling.
 * @param {!Event} event
 * @param {!HTMLElement} page
 */
export function showScrollingEffects(event, page) {
  const shadowShield = page.shadowRoot.querySelector('#shadowShield');
  const content = page.shadowRoot.querySelector('#content');
  const navButtons = page.shadowRoot.querySelector('#navButtons');
  const shadowElevation = page.shadowRoot.querySelector('#shadowElevation');
  const separator = page.shadowRoot.querySelector('#separator');

  shadowElevation.classList.toggle(
      'elevation-shadow-scrolling', content.scrollTop > 0);
  navButtons.classList.toggle('nav-buttons-scrolling', content.scrollTop > 0);
  shadowShield.classList.toggle(
      'scrolling-shield',
      content.scrollTop + content.clientHeight < content.scrollHeight);
  separator.classList.toggle(
      'separator-scrolling-end',
      content.scrollTop + content.clientHeight == content.scrollHeight &&
          content.scrollTop > 0);
}

/**
 * Show initial effect only when the content is scrollable.
 * @param {!HTMLElement} page
 */
export function showScrollingEffectOnStart(page) {
  const content = page.shadowRoot.querySelector('#content');
  const shadowShield = page.shadowRoot.querySelector('#shadowShield');
  shadowShield.classList.toggle(
      'scrolling-shield', content.scrollHeight > content.clientHeight);
}
