// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

export interface Color {
  background: SkColor;
  foreground: SkColor;
}

export const LIGHT_DEFAULT_COLOR: Color = {
  background: {value: 0xffffffff},
  foreground: {value: 0xffdee1e6},
};

export const DARK_DEFAULT_COLOR: Color = {
  background: {value: 0xff323639},
  foreground: {value: 0xff202124},
};

export enum ColorType {
  NONE,
  DEFAULT,
  MAIN,
  CHROME,
  CUSTOM,
}

export interface SelectedColor {
  type: ColorType;
  chromeColor?: SkColor;
}
