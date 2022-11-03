// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WallpaperCalculatedColors, WallpaperColorsHandler, WallpaperColorsObserverCallbackRouter} from '/ash/webui/color_internals/mojom/color_internals.mojom-webui.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {getRGBAFromComputedStyle} from './utils.js';

function rmChildren(node: HTMLElement) {
  while (node.lastChild) {
    node.removeChild(node.lastChild);
  }
}

/**
 * SkColor is an integer whose bits represent a color in ARGB.
 * Convert to a hex string in RGBA for HTML.
 */
function skColorToHexStr(color: SkColor): string {
  const rgb = (color.value & 0x0FFFFFF).toString(16).padStart(6, '0');
  const alpha = (color.value >>> 24).toString(16).padStart(2, '0');
  return `#${rgb}${alpha}`;
}

function createRow(color: SkColor) {
  const div = document.createElement('div');
  div.classList.add('wallpaper-color-container');
  const span = document.createElement('span');
  span.classList.add('color-swatch');
  span.style.backgroundColor = skColorToHexStr(color);
  const rgbaText = document.createElement('span');
  rgbaText.classList.add('wallpaper-color-text');

  div.appendChild(span);
  div.appendChild(rgbaText);
  return div;
}

function writeRgbaText() {
  const elements = document.querySelectorAll('.wallpaper-color-container');
  for (const elem of elements) {
    const rgbaNode = elem.querySelector('.wallpaper-color-text');
    const rgba = getRGBAFromComputedStyle(elem.querySelector('.color-swatch')!);
    rgbaNode!.textContent = rgba;
  }
}

function handleWallpaperColorChanged(colors: WallpaperCalculatedColors) {
  document.getElementById('wallpaper-block')!.setAttribute('loading', '');

  const prominentContainer =
      document.getElementById('wallpaper-prominent-colors-container');
  rmChildren(prominentContainer!);
  colors.prominentColors.map(createRow).forEach(
      row => prominentContainer!.appendChild(row));

  const kMeanContainer =
      document.getElementById('wallpaper-k-mean-color-container');
  rmChildren(kMeanContainer!);

  kMeanContainer!.appendChild(createRow(colors.kMeanColor));

  writeRgbaText();

  document.getElementById('wallpaper-block')!.removeAttribute('loading');
}

export function startObservingWallpaperColors() {
  const remote = WallpaperColorsHandler.getRemote();
  const callbackRouter = new WallpaperColorsObserverCallbackRouter();
  callbackRouter.onWallpaperColorsChanged.addListener(
      handleWallpaperColorChanged);
  remote.setWallpaperColorsObserver(
      callbackRouter.$.bindNewPipeAndPassRemote());
}
