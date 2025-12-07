// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Information about a particular theme color to be displayed.
export interface ThemeData {
  // The CSS variable name of the color to be demo'd.
  background: string;
  // The foreground color to use for contrast. If not specified, defaults to
  // '--color-on-surface'.
  foreground?: string;
}

// Delineates a section containing related colors.
export interface ThemeSection {
  // The section title to be displayed.
  title: string;
  // The colors to be displayed within the section.
  entries: ThemeData[];
}

// The list of all sections and colors to be displayed.
//
// Ideally, this will be all solid colors in the CDDS Design Kit; specifically
// those listed under "Foundations / Color". This guarantees that all of these
// colors are part of the approved design language from which new surfaces -
// WebUI or Views - should be constructed.
export const ALL_SECTIONS: ThemeSection[] = [
  {
    title: 'Universal',
    entries: [
      {
        background: '--color-sys-surface',
      },
      {
        background: '--color-sys-surface1',
      },
      {
        background: '--color-sys-surface2',
      },
      {
        background: '--color-sys-surface3',
      },
      {
        background: '--color-sys-surface4',
      },
      {
        background: '--color-sys-on-surface',
        foreground: '--color-sys-surface',
      },
      {
        background: '--color-sys-on-surface-subtle',
        foreground: '--color-sys-surface',
      },
      {
        background: '--color-sys-on-surface-primary',
        foreground: '--color-sys-surface',
      },
    ],
  },
  {
    title: 'Accent',
    entries: [
      {
        background: '--color-sys-primary',
        foreground: '--color-sys-on-primary',
      },
      {
        background: '--color-sys-on-primary',
        foreground: '--color-sys-primary',
      },
      {
        background: '--color-sys-secondary',
        foreground: '--color-sys-on-secondary',
      },
      {
        background: '--color-sys-on-secondary',
        foreground: '--color-sys-secondary',
      },
      {
        background: '--color-sys-tertiary',
        foreground: '--color-sys-on-tertiary',
      },
      {
        background: '--color-sys-on-tertiary',
        foreground: '--color-sys-tertiary',
      },
      {
        background: '--color-sys-error',
        foreground: '--color-sys-on-error',
      },
      {
        background: '--color-sys-on-error',
        foreground: '--color-sys-error',
      },
    ],
  },
  {
    title: 'Containers',
    entries: [
      {
        background: '--color-sys-tonal-container',
        foreground: '--color-sys-on-tonal-container',
      },
      {
        background: '--color-sys-on-tonal-container',
        foreground: '--color-sys-tonal-container',
      },
      {
        background: '--color-sys-base-tonal-container',
        foreground: '--color-sys-on-base-tonal-container',
      },
      {
        background: '--color-sys-on-base-tonal-container',
        foreground: '--color-sys-base-tonal-container',
      },
      {
        background: '--color-sys-tertiary-container',
        foreground: '--color-sys-on-tertiary-container',
      },
      {
        background: '--color-sys-on-tertiary-container',
        foreground: '--color-sys-tertiary-container',
      },
      {
        background: '--color-sys-error-container',
        foreground: '--color-sys-on-error-container',
      },
      {
        background: '--color-sys-on-error-container',
        foreground: '--color-sys-error-container',
      },
      {
        background: '--color-sys-neutral-container',
      },
    ],
  },
  {
    title: 'Base',
    entries: [
      {
        background: '--color-sys-base',
      },
      {
        background: '--color-sys-base-container',
      },
      {
        background: '--color-sys-base-container-elevated',
      },
    ],
  },
  {
    title: 'Header',
    entries: [
      {background: '--color-sys-header'},
      {background: '--color-sys-header-container'},
      {
        background: '--color-sys-on-header-primary',
        foreground: '--color-sys-header',
      },
      {
        background: '--color-sys-on-header-divider',
      },
    ],
  },
  {
    title: 'Inverse',
    entries: [
      {
        background: '--color-sys-inverse-surface',
        foreground: '--color-sys-inverse-on-surface',
      },
      {
        background: '--color-sys-inverse-primary',
        foreground: '--color-sys-inverse-surface',
      },
      {
        background: '--color-sys-inverse-on-surface',
        foreground: '--color-sys-inverse-surface',
      },
    ],
  },
  {
    title: 'Outline',
    entries: [
      {
        background: '--color-sys-tonal-outline',
      },
      {
        background: '--color-sys-neutral-outline',
      },
      {
        background: '--color-sys-divider',
      },
    ],
  },
  {
    title: 'States',
    entries: [
      {
        background: '--color-sys-state-hover-on-prominent',
      },
      {
        background: '--color-sys-state-hover-on-subtle',
      },
      {
        background: '--color-sys-state-focus-ring',
        foreground: '--color-sys-surface',
      },
      {
        background: '--color-sys-state-focus-highlight',
      },
      {
        background: '--color-sys-state-text-highlight',
        foreground: '--color-sys-state-on-text-highlight',
      },
      {
        background: '--color-sys-state-on-text-highlight',
      },
      {
        background: '--color-sys-state-disabled',
        foreground: '--color-sys-surface',
      },
      {
        background: '--color-sys-state-disabled-container',
      },
    ],
  },
];
