// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {skColorToHexColor, skColorToRgba} from '//resources/js/color_utils.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import type {OverlayTheme} from './lens.mojom-webui.js';

// Returns the Fallback theme. Used to initialize the theme at load time.
export function getFallbackTheme(): OverlayTheme {
  return {
    primary: {
      value: loadTimeData.getInteger('colorFallbackPrimary'),
    },
    shaderLayer1: {
      value: loadTimeData.getInteger('colorFallbackShaderLayer1'),
    },
    shaderLayer2: {
      value: loadTimeData.getInteger('colorFallbackShaderLayer2'),
    },
    shaderLayer3: {
      value: loadTimeData.getInteger('colorFallbackShaderLayer3'),
    },
    shaderLayer4: {
      value: loadTimeData.getInteger('colorFallbackShaderLayer4'),
    },
    shaderLayer5: {
      value: loadTimeData.getInteger('colorFallbackShaderLayer5'),
    },
    scrim: {
      value: loadTimeData.getInteger('colorFallbackScrim'),
    },
    surfaceContainerHighestLight: {
      value:
          loadTimeData.getInteger('colorFallbackSurfaceContainerHighestLight'),
    },
    surfaceContainerHighestDark: {
      value:
          loadTimeData.getInteger('colorFallbackSurfaceContainerHighestDark'),
    },
    selectionElement: {
      value: loadTimeData.getInteger('colorFallbackSelectionElement'),
    },
  };
}

/**
 * Converts an SkColor object to a string in the form
 * "rgba(<red>, <green>, <blue>, <alpha>)", with a custom
 * alpha value.
 * @param skColor The input color.
 * @return The rgba string.
 */
export function skColorToRgbaWithCustomAlpha(
    skColor: SkColor, alpha: number): string {
  const r = (skColor.value >> 16) & 0xff;
  const g = (skColor.value >> 8) & 0xff;
  const b = skColor.value & 0xff;
  return `rgba(${r}, ${g}, ${b}, ${alpha.toFixed(2)})`;
}

// Returns an array of the Shader hex colors from the given theme.
export function getShaderLayerColorHexes(theme: OverlayTheme): string[] {
  return [
    skColorToHexColor(theme.shaderLayer1),
    skColorToHexColor(theme.shaderLayer2),
    skColorToHexColor(theme.shaderLayer3),
    skColorToHexColor(theme.shaderLayer4),
    skColorToHexColor(theme.shaderLayer5),
  ];
}

// Returns an array of the Shader rgba colors from the given theme.
export function getShaderLayerColorRgbas(theme: OverlayTheme): string[] {
  return [
    skColorToRgba(theme.shaderLayer1),
    skColorToRgba(theme.shaderLayer2),
    skColorToRgba(theme.shaderLayer3),
    skColorToRgba(theme.shaderLayer4),
    skColorToRgba(theme.shaderLayer5),
  ];
}

// Returns a new rgba string using the rgb values from |rgba| and the alpha
// value provided.
export function modifyRgbaTransparency(rgba: string, alpha: number): string {
  const colorsString: string = rgba.substring(5, rgba.length - 1);
  const [red, green, blue] = colorsString.split(',');
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}
