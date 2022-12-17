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

  // Debugging found the scrollTop reading sometimes was off by 0.61 px when the
  // content was scrolled to bottom. This caused the shadowShield be still
  // visible even though it should not be. Relax the condition to be not larger
  // than 1 px.
  const scrollingEnded =
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
 * @param {!HTMLElement} page
 */
export function showScrollingEffectOnStart(page) {
  const content = page.shadowRoot.querySelector('#content');
  const shadowShield = page.shadowRoot.querySelector('#shadowShield');
  shadowShield.classList.toggle(
      'scrolling-shield', content.scrollHeight > content.clientHeight);
}
