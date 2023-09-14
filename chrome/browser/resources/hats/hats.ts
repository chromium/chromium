// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Global `window` context.
declare global {
  interface Window {
    help: any;
    initializeHats: () => void;
  }
}

function initialize() {
  const script = document.createElement('script');
  script.type = 'text/javascript';
  script.src =
      'https://www.gstatic.com/feedback/js/help/prod/service/lazy.min.js';
  script.onload = requestSurvey;
  document.head.appendChild(script);
}

function requestSurvey() {
  // Provide a dummy window size, such that the survey renders at its desired
  // size. The actual dialog will be resized to the survey provided size
  // transparently.
  window.innerWidth = 800;
  window.innerHeight = 600;

  const helpApi = window.help.service.Lazy.create(
      0, {apiKey: 'HATS_API_KEY', locale: 'en-US'});

  let loadedSent = false;

  const surveyListener = {
    // Provide survey state to the WebContentsObserver via URL fragments.
    surveyClosed: function() {
      history.pushState('', '', '#close');
    },

    surveyPositioning: function(_: any, size: any, animationSpec: any) {
      // Delay sending a loaded signal to the browser until the initial
      // animation has completed.
      if (!loadedSent) {
        setTimeout(function() {
          history.pushState('', '', '#loaded');
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

  const params = new URLSearchParams(window.location.search);

  helpApi.requestSurvey({
    triggerId: params.get('trigger_id'),
    enableTestingMode: !!params.get('enable_testing'),
    preferredSurveyLanguageList:
        JSON.parse(decodeURIComponent(params.get('languages')!)),
    callback: (requestSurveyCallbackParam: any) => {
      if (!requestSurveyCallbackParam.surveyData) {
        history.pushState('', '', '#close');
        return;
      }
      helpApi.presentSurvey({
        surveyData: requestSurveyCallbackParam.surveyData,
        colorScheme: window.matchMedia('(prefers-color-scheme: dark)').matches ?
            /* dark */ 2 :
            /* light */ 1,
        customZIndex: 10000,
        customLogoUrl: 'https://www.gstatic.com/images' +
            '/branding/product/2x/chrome_48dp.png',
        listener: surveyListener,
        productData: {
          customData: JSON.parse(
              decodeURIComponent(params.get('product_specific_data')!)),
        },
      });
    },
  });
}

window.initializeHats = initialize;

export {};
