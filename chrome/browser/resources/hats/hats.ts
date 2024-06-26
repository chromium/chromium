// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js';

// Types that Chrome provides, but aren't standard TS.
declare global {
  interface Window {
    help: any;
  }

  interface Navigator {
    userAgentData: any;
  }
}

let browserProxy: BrowserProxy|null = null;

// Wait for the external script to be loaded, before establishing a mojo
// connection with the browser process. The browser process will be the one
// driving the events after that.
initializeExternalScript().then(() => {
  if (!browserProxy) {
    browserProxy = new BrowserProxy(requestSurvey);
  }
});

function initializeExternalScript() {
  const script = document.createElement('script');
  script.type = 'text/javascript';
  script.src =
      'https://www.gstatic.com/feedback/js/help/prod/service/lazy.min.js';
  const loaded = new Promise(r => script.onload = r);
  document.head.appendChild(script);
  return loaded;
}

async function requestSurvey(
    apiKey: string, triggerId: string, enableTesting: boolean,
    languageList: string[], productSpecificDataJson: string) {
  // Provide a dummy window size, such that the survey renders at its desired
  // size. The actual dialog will be resized to the survey provided size
  // transparently.
  window.innerWidth = 800;
  window.innerHeight = 600;

  const helpApi =
      window.help.service.Lazy.create(0, {apiKey: apiKey, locale: 'en-US'});

  let loadedSent = false;

  const surveyListener = {
    surveyClosed: function() {
      browserProxy!.handler.onSurveyClosed();
    },

    surveyCompleted: function() {
      browserProxy!.handler.onSurveyCompleted();
    },

    surveyPositioning: function(_: any, size: any, animationSpec: any) {
      // Delay sending a loaded signal to the browser until the initial
      // animation has completed.
      if (!loadedSent) {
        setTimeout(function() {
          browserProxy!.handler.onSurveyLoaded();
        }, animationSpec.duration * 1000);
        loadedSent = true;
      }
      // Resize the wrapping div to the requested survey size. The web
      // contents displaying this content will resize to fit.
      const wrapper: HTMLElement = document.querySelector('#surveyWrapper')!;
      wrapper.style.width = `${size.width}px`;
      wrapper.style.height = `${size.height}px`;

      return {
        anchor: 3,
        verticalMargin: 0.1,
        horizontalMargin: 0.1,
      };
    },
  };

  // High contrast mode on Windows will present as dark mode, but does not
  // work with the dark mode survey (see b/343275531) as the colors Windows
  // chooses hide some elements.
  const isHighContrast = window.matchMedia('(forced-colors: active)').matches;
  const isWindows = navigator.userAgentData.platform === 'Windows';
  const isDarkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;

  const requestDarkMode = isDarkMode && !(isWindows && isHighContrast);

  helpApi.requestSurvey({
    triggerId: triggerId,
    enableTestingMode: enableTesting,
    preferredSurveyLanguageList: languageList,
    callback: (requestSurveyCallbackParam: any) => {
      if (!requestSurveyCallbackParam.surveyData) {
        browserProxy!.handler.onSurveyClosed();
        return;
      }
      helpApi.presentSurvey({
        surveyData: requestSurveyCallbackParam.surveyData,
        colorScheme: requestDarkMode ? /* dark */ 2 : /* light */ 1,
        customZIndex: 10000,
        customLogoUrl: 'https://www.gstatic.com/images' +
            '/branding/product/2x/chrome_48dp.png',
        listener: surveyListener,
        productData: {
          customData: productSpecificDataJson,
        },
      });
    },
  });
}

export {};
