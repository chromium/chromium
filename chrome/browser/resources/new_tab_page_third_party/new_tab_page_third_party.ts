// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/most_visited/most_visited.js';

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';

import {BrowserProxy} from './browser_proxy.js';
import {Theme} from './new_tab_page_third_party.mojom-webui.js';

const {callbackRouter, handler} = BrowserProxy.getInstance();

callbackRouter.setTheme.addListener((theme: Theme) => {
  const html = document.documentElement;
  html.toggleAttribute('has-custom-background', theme.hasCustomBackground);
  const style = html.style;
  style.backgroundColor = theme.colorBackground;
  const backgroundImage = `-webkit-image-set(
      url(chrome://theme/IDR_THEME_NTP_BACKGROUND?${theme.id}) 1x,
      url(chrome://theme/IDR_THEME_NTP_BACKGROUND@2x?${theme.id}) 2x)`;
  style.backgroundImage = theme.hasCustomBackground ? backgroundImage : 'unset';
  style.backgroundRepeat = theme.backgroundTiling;
  style.backgroundPosition = theme.backgroundPosition;
  style.setProperty('--ntp-theme-text-color', skColorToRgba(theme.textColor));

  const mostVisitedElement = document.querySelector('cr-most-visited')!;
  mostVisitedElement.set('theme', theme.mostVisited);
});
handler.updateTheme();
