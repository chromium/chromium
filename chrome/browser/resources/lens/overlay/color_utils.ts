// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {skColorToHexColor} from '//resources/js/color_utils.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

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
