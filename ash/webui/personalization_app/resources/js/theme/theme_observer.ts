// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {ColorScheme} from '../../color_scheme.mojom-webui.js';
import {SampleColorScheme, ThemeObserverInterface, ThemeObserverReceiver, ThemeProviderInterface} from '../../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';

import {setColorModeAutoScheduleEnabledAction, setColorSchemeAction, setDarkModeEnabledAction, setSampleColorSchemesAction, setStaticColorAction} from './theme_actions.js';
import {getThemeProvider} from './theme_interface_provider.js';

/** @fileoverview listens for updates on color mode changes. */

let instance: ThemeObserver|null = null;

/**
 * Observes color mode changes and saves updates to PersonalizationStore.
 */
export class ThemeObserver implements ThemeObserverInterface {
  static initThemeObserverIfNeeded(): void {
    if (!instance) {
      instance = new ThemeObserver();
    }
  }

  static shutdown() {
    if (instance) {
      instance.receiver_.$.close();
      instance = null;
    }
  }

  private receiver_: ThemeObserverReceiver =
      this.initReceiver_(getThemeProvider());

  private initReceiver_(themeProvider: ThemeProviderInterface):
      ThemeObserverReceiver {
    const receiver = new ThemeObserverReceiver(this);
    themeProvider.setThemeObserver(receiver.$.bindNewPipeAndPassRemote());
    return receiver;
  }

  onColorModeChanged(darkModeEnabled: boolean): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
  }

  onColorModeAutoScheduleChanged(enabled: boolean): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setColorModeAutoScheduleEnabledAction(enabled));
  }

  onColorSchemeChanged(scheme: ColorScheme): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setColorSchemeAction(scheme));
  }

  onSampleColorSchemesChanged(sampleColorSchemes: SampleColorScheme[]): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setSampleColorSchemesAction(sampleColorSchemes));
  }

  onStaticColorChanged(staticColor: SkColor): void {
    const store = PersonalizationStore.getInstance();
    store.dispatch(setStaticColorAction(staticColor));
  }
}
