// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common styles for polymer components in both trusted and
 * untrusted code. Polymer must be imported before this file. This file cannot
 * import Polymer itself because trusted and untrusted code access polymer at
 * different paths.
 */
const styles = document.createElement('dom-module');

styles.innerHTML = `
<template>
  <style>
    :host {
      --personalization-app-grid-item-border-radius: 12px;
      --personalization-app-grid-item-height: 120px;
      --personalization-app-grid-item-spacing: 16px;

      --personalization-app-text-shadow-elevation-1: 0 1px 3px
          rgba(0, 0, 0, 15%), 0 1px 2px rgba(0, 0, 0, 30%);

      /* copied from |AshColorProvider| |kSecondToneOpacity| constant. */
      --personalization-app-second-tone-opacity: 0.3;
    }
    @media (prefers-color-scheme: light) {
      .placeholder,
      .photo-images-container {
        background-color: var(--google-grey-100);
      }
    }
    @media (prefers-color-scheme: dark) {
      .placeholder,
      .photo-images-container {
        background-color: rgba(var(--google-grey-700-rgb), 0.3);
      }
    }
    iron-list {
      height: 100%;
    }
    .photo-container {
      box-sizing: border-box;
      height: calc(
        var(--personalization-app-grid-item-height) +
        var(--personalization-app-grid-item-spacing));
      overflow: hidden;
      padding: calc(var(--personalization-app-grid-item-spacing) / 2);
      /* Media queries in trusted and untrusted code will resize to 25% at
       * correct widths.  Subtract 0.34px to fix subpixel rounding issues with
       * iron-list. This makes sure all photo containers on a row add up to at
       * least 1px smaller than the parent width.*/
      width: calc(100% / 3 - 0.34px);
    }
    .photo-container:focus-visible {
      outline: none;
    }
    /* This extra position: relative element corrects for absolutely positioned
       elements ignoring parent interior padding. */
    .photo-inner-container {
      align-items: center;
      border-radius: var(--personalization-app-grid-item-border-radius);
      display: flex;
      cursor: pointer;
      height: 100%;
      justify-content: center;
      overflow: hidden;
      position: relative;
      width: 100%;
    }
    .photo-inner-container.photo-loading-failure {
      cursor: default;
      filter: grayscale(100%);
      opacity: 0.3;
    }
    .photo-inner-container.photo-empty:not([selectable]) {
      cursor: default;
    }
    @keyframes ripple {
      /* 0 ms */
      from {
        opacity: 1;
      }
      /* 200 ms */
      9% {
        opacity: 0.15;
      }
      /* 350 ms */
      15.8% {
        opacity: 0.15;
      }
      /* 550 ms, hold for 83ms * 20 and then restart */
      24.9% {
        opacity: 1;
      }
      /* 2210 ms */
      to {
        opacity: 1;
      }
    }
    .placeholder {
      animation: 2210ms linear var(--animation-delay, 1s) infinite ripple;
    }
    .photo-inner-container:focus-visible,
    .photo-loading-placeholder:focus-visible {
      border: 2px solid var(--cros-focus-ring-color);
      border-radius: 14px;
      outline: none;
    }
    .photo-images-container {
      border: 1px solid rgba(0, 0, 0, 0.08);
      border-radius: 12px;
      box-sizing: border-box;
      display: flex;
      flex-flow: row wrap;
      height: 100%;
      /* stop img and gradient-mask from ignoring above border-radius. */
      overflow: hidden;
      width: 100%;
    }
    .photo-images-container.photo-images-container-0 {
      background-color: var(--cros-highlight-color);
      justify-content: center;
      align-items: flex-start;
    }
    .photo-images-container img {
      flex: 1 1 0;
      height: 100%;
      min-width: 50%;
      object-fit: cover;
      width: 100%;
    }
    .photo-images-container.photo-images-container-3 img {
      height: 50%;
    }
    .photo-images-container.photo-images-container-0 img {
      object-fit: scale-down;
      flex: 0 1 0;
      height: initial;
      width: initial;
      min-width: initial;
      margin: 12px;
    }
    @keyframes scale-up {
      from {
        transform: scale(0);
      }
      to {
        transform: scale(1);
      }
    }
    .photo-container iron-icon[icon='personalization:checkmark'] {
      --iron-icon-height: 20px;
      --iron-icon-width: 20px;
      left: 8px;
      position: absolute;
      top: 8px;
      animation-name: scale-up;
      animation-duration: 200ms;
      animation-timing-functino: cubic-bezier(0.40, 0.00, 0.20, 1.00);
    }
    .photo-inner-container:not([aria-selected='true'])
    iron-icon[icon='personalization:checkmark'] {
      display: none;
    }
    .photo-inner-container[aria-selected='true'] {
      background-color: rgba(var(--cros-icon-color-prominent-rgb),
          var(--personalization-app-second-tone-opacity));
      border-radius: 16px;
    }
    @keyframes resize {
      100% {
        height: calc(100% - 8px);
        width: calc(100% - 8px);
      }
    }
    .photo-inner-container[aria-selected='true'] .photo-images-container {
      animation-name: resize;
      animation-duration: 200ms;
      animation-fill-mode: forwards;
      animation-timing-function: cubic-bezier(0.40, 0.00, 0.20, 1.00);
    }
    .photo-inner-container:focus-visible:not([aria-selected='true'])
    .photo-images-container {
      height: calc(100% - 4px);
      width: calc(100% - 4px);
    }
    .photo-text-container {
      box-sizing: border-box;
      bottom: 8px;
      position: absolute;
      width: 100%;
      z-index: 2;
    }
    .photo-text-container > p {
      color: white;
      font: var(--cros-annotation-2-font);
      margin: 0;
      max-width: 100%;
      overflow: hidden;
      text-align: center;
      text-overflow: ellipsis;
      text-shadow: var(--personalization-app-text-shadow-elevation-1);
      white-space: nowrap;
    }
    .photo-text-container > p:first-child {
      font: var(--cros-headline-1-font);
    }
    .photo-empty .photo-text-container > p {
      color: var(--cros-button-label-color-secondary);
      text-shadow: none;
    }
    .photo-gradient-mask {
      border-radius: 12px;
      position: absolute;
      top: 50%;
      left: 0;
      width: 100%;
      height: 50%;
      z-index: 1;
      background: linear-gradient(rgba(var(--google-grey-900-rgb), 0%),
          rgba(var(--google-grey-900-rgb), 55%));
    }
  </style>
</template>`;

styles.register('common-style');
