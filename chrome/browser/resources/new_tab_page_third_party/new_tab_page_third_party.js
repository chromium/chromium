// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './most_visited.js';

import {BrowserProxy} from './browser_proxy.js';

const {callbackRouter, handler} = BrowserProxy.getInstance();

callbackRouter.setTheme.addListener(theme => {
  const html = document.documentElement;
  html.toggleAttribute('has-custom-background', theme.hasCustomBackground);
  html.toggleAttribute('dark', theme.isDark);
  const style = html.style;
  style.backgroundColor = theme.colorBackground;
  const backgroundImage = `-webkit-image-set(
      url(chrome://theme/IDR_THEME_NTP_BACKGROUND?${theme.id}) 1x,
      url(chrome://theme/IDR_THEME_NTP_BACKGROUND@2x?${theme.id}) 2x)`;
  style.backgroundImage = theme.hasCustomBackground ? backgroundImage : 'unset';
  style.backgroundRepeat = theme.backgroundTiling;
  style.backgroundPosition = theme.backgroundPosition;
});
handler.updateTheme();

export {BrowserProxy};
