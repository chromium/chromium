// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type MessageType = 'overlaysUpdated'|'click'|'loaded';

// TODO(crbug.com/373569279): Post launch completion of OGB ABP integration,
// remove all references and logic associated to the legacy integration
// implementation.
declare let abp: boolean;

/**
 * The following |messageType|'s are sent to the parent frame:
 *  - loaded: sent on initial load.
 *  - overlaysUpdated: sent when an overlay is updated. The overlay bounding
 *        rects are included in the |data|.
 *  - click: sent when the OGB was clicked.
 */
function postMessage(messageType: MessageType, data?: object) {
  if (window === window.parent) {
    return;
  }
  window.parent.postMessage(
      {frameType: 'one-google-bar', messageType, data},
      'chrome://new-tab-page');
}

interface Bar {
  setDarkMode(matches: boolean): void;
  setBackgroundColor(bgColor: string): void;
  setForegroundStyle(style: number): void;
}

interface AsyncBar {
  setDarkMode(matches: boolean): void;
}

type IndexableApi = Record<string, Function>;
interface Gbar {
  gbar?: {a: Record<string, () => IndexableApi>, P: () => void};
}

if (abp) {
  window.addEventListener('gbar_a', () => {
    postOneGoogleBarLoaded();
  });
}

const oneGoogleBarApi = (() => {
  async function callApi(
      apiName: string, fnName: string, ...args: any[]): Promise<unknown> {
    const {gbar} = window as Window & Gbar;
    if (!gbar) {
      return;
    }
    const api = await gbar.a[apiName]!();
    return api[fnName]!.apply(api, args);
  }

  async function callAsyncBarApi(
      fnName: string, ...args: any[]): Promise<unknown> {
    const {gbar} = window as Window & Gbar;
    if (!gbar || !gbar.a) {
      return Promise.resolve();
    }

    const barApi = await gbar.a['bf']!();
    return barApi[fnName]!.apply(barApi, args);
  }

  interface Definition {
    name: string;
    apiName: string;
    fns: Array<[string, string]>;
  }

  const api: {bar: Bar} = [
    {
      name: 'bar',
      apiName: 'bf',
      fns: [
        ['setForegroundStyle', 'pc'],
        ['setBackgroundColor', 'pd'],
        ['setDarkMode', 'pp'],
      ],
    } as Definition,
  ].reduce((topLevelApi, def) => {
    (topLevelApi as Record<string, any>)[def.name] =
        def.fns.reduce((apiPart, [name, fnName]) => {
          apiPart[name] = callApi.bind(null, def.apiName, fnName);
          return apiPart;
        }, {} as IndexableApi);
    return topLevelApi;
  }, {} as {bar: Bar});

  const asyncBar: AsyncBar =
      [['setDarkMode', 'pp']].reduce((bar: any, [name, fnName]) => {
        bar[name!] = callAsyncBarApi.bind(null, fnName!);
        return bar;
      }, {} as Bar);

  async function updateDarkMode(): Promise<void> {
    if (abp) {
      await asyncBar.setDarkMode(
          window.matchMedia('(prefers-color-scheme: dark)').matches);
    } else {
      await api.bar.setDarkMode(
          window.matchMedia('(prefers-color-scheme: dark)').matches);
      // |setDarkMode(toggle)| updates the background color and foreground
      // style. The background color should always be 'transparent'.
      api.bar.setBackgroundColor('transparent');
      // The foreground style is set based on NTP theme and not dark mode.
      api.bar.setForegroundStyle(foregroundLight ? 1 : 0);
    }
  }

  let foregroundLight: boolean = false;

  return {
    /**
     * Updates the foreground on the OneGoogleBar to provide contrast against
     * the background.
     */
    setForegroundLight: (enabled: boolean) => {
      if (abp) {
        asyncBar.setDarkMode(enabled);
      } else if (foregroundLight !== enabled) {
        foregroundLight = enabled;
        api.bar.setForegroundStyle(foregroundLight ? 1 : 0);
      }
    },

    /**
     * Updates the OneGoogleBar dark mode when called as well as any time dark
     * mode is updated.
     */
    trackDarkModeChanges: async () => {
      window.matchMedia('(prefers-color-scheme: dark)').addListener(() => {
        updateDarkMode();
      });
      await updateDarkMode();
    },
  };
})();

/**
 * Object that exposes:
 *  - |track()|: sets up MutationObserver to track element visibility changes.
 *  - |update(potentialNewOverlays)|: determines visibility of tracked elements
 *        and sends an update to the top frame about element visibility.
 */
const overlayUpdater = (() => {
  const overlays = new Set<HTMLElement>();
  let lastOverlayRects: DOMRect[] = [];
  let elementsTransitioningCount: number = 0;
  let updateIntervalId: number|null = null;
  let initialElementsAdded: boolean = false;

  function transitionStart() {
    elementsTransitioningCount++;
    if (!updateIntervalId) {
      updateIntervalId = setInterval(() => {
        update([]);
      });
    }
  }

  function transitionStop() {
    if (elementsTransitioningCount > 0) {
      elementsTransitioningCount--;
    }
    if (updateIntervalId && elementsTransitioningCount === 0) {
      clearInterval(updateIntervalId);
      updateIntervalId = null;
    }
  }

  function addOverlay(overlay: HTMLElement) {
    if (overlays.has(overlay)) {
      return;
    }
    // If an overlay starts a transition, the updated bounding rects need to
    // be sent to the top frame during the transition. The MutationObserver
    // will only handle new elements and changes to the element attributes.
    overlay.addEventListener('animationstart', transitionStart);
    overlay.addEventListener('animationend', transitionStop);
    overlay.addEventListener('animationcancel', transitionStop);
    overlay.addEventListener('transitionstart', transitionStart);
    overlay.addEventListener('transitionend', transitionStop);
    overlay.addEventListener('transitioncancel', transitionStop);
    // Update links that are loaded dynamically to ensure target is "_blank"
    // or "_top".
    // TODO(crbug.com/40667075): remove after OneGoogleBar links are updated.
    overlay.parentElement!.querySelectorAll('a').forEach(el => {
      if (el.target !== '_blank' && el.target !== '_top') {
        el.target = '_top';
      }
    });
    const {transition} = getComputedStyle(overlay);
    const opacityTransition = 'opacity 0.1s ease 0.02s';
    // Check if the transition is the default computed transition style. If it
    // is not, append to the existing transitions.
    if (transition === 'all 0s ease 0s') {
      overlay.style.transition = opacityTransition;
    } else if (!transition.includes('opacity')) {
      overlay.style.transition = transition + ', ' + opacityTransition;
    }
    // The element has an initial opacity of 1. If the element is being added to
    // |overlays| and shown in the same |update()| call, the opacity transition
    // will not work since the opacity is already 1. For this reason the
    // 'fade-in' class is added to the element which runs an initial fade-in
    // animation.
    overlay.classList.add('fade-in');
    overlays.add(overlay);
  }

  function update(potentialNewOverlays: HTMLElement[]) {
    const gbElement = document.body.querySelector('#gb');
    if (!gbElement) {
      return;
    }
    const barRect = gbElement.getBoundingClientRect();
    if (barRect.bottom === 0) {
      return;
    }
    // After loaded, there could exist overlays that are shown, but not
    // mutated. Add all elements that could be an overlay. The children of the
    // actual overlay element are removed before sending any overlay update
    // message.
    if (!initialElementsAdded) {
      initialElementsAdded = true;
      Array.from(document.body.querySelectorAll<HTMLElement>('*'))
          .forEach(el => {
            potentialNewOverlays.push(el);
          });
    }
    Array.from(potentialNewOverlays).forEach(overlay => {
      const rect = overlay.getBoundingClientRect();
      if (overlay.parentElement && rect.width > 0 &&
          rect.bottom > barRect.bottom) {
        addOverlay(overlay);
      }
    });
    // Remove overlays detached from DOM.
    Array.from(overlays).forEach(overlay => {
      if (!overlay.parentElement) {
        overlays.delete(overlay);
      }
    });
    // Check if an overlay and its parents are visible.
    const overlayRects: DOMRect[] = [];
    overlays.forEach(overlay => {
      const {display, visibility} = window.getComputedStyle(overlay);
      const rect = overlay.getBoundingClientRect();
      const shown = display !== 'none' && visibility !== 'hidden' &&
          rect.bottom > barRect.bottom;
      if (shown) {
        overlayRects.push(rect);
      }
      // Setting the style here avoids triggering the mutation observer.
      overlay.style.opacity = shown ? '1' : '0';
    });
    overlayRects.push(barRect);
    const noChange = overlayRects.length === lastOverlayRects.length &&
        lastOverlayRects.every((rect, i) => {
          const newRect = overlayRects[i]!;
          return newRect.left === rect.left && newRect.top === rect.top &&
              newRect.right === rect.right && newRect.bottom === rect.bottom;
        });
    lastOverlayRects = overlayRects;
    if (noChange) {
      return;
    }
    postMessage('overlaysUpdated', overlayRects);
  }

  function track() {
    const observer = new MutationObserver(mutations => {
      const potentialNewOverlays: HTMLElement[] = [];
      // Add any mutated element that is an overlay to |overlays|.
      mutations.forEach(({target}) => {
        if (overlays.has(target as HTMLElement) || !target.parentElement) {
          return;
        }
        potentialNewOverlays.push(target as HTMLElement);
      });
      update(potentialNewOverlays);
    });
    observer.observe(document.body, {
      attributes: true,
      childList: true,
      subtree: true,
    });
  }

  return {track, update};
})();

window.addEventListener('message', ({data}) => {
  if (data.type === 'updateAppearance') {
    oneGoogleBarApi.setForegroundLight(data.applyLightTheme);
  }
});

// Need to send overlay updates on resize because overlay bounding rects are
// absolutely positioned.
window.addEventListener('resize', () => {
  overlayUpdater.update([]);
});

// When the account overlay is shown, it does not close on blur. It does close
// when focusing the body.
window.addEventListener('blur', e => {
  if (e.target === window && document.activeElement === document.body) {
    document.body.focus();
  }
});

window.addEventListener('click', () => {
  postMessage('click');
}, /*useCapture=*/ true);

function postOneGoogleBarLoaded() {
  postMessage('loaded');
  overlayUpdater.track();
  oneGoogleBarApi.trackDarkModeChanges();
}

if (!abp) {
  document.addEventListener('DOMContentLoaded', () => {
    document.body.style.margin = '0';
    // TODO(crbug.com/40667075): remove after OneGoogleBar links are updated.
    // Updates <a>'s so they load on the top frame instead of the iframe.
    document.body.querySelectorAll('a').forEach(el => {
      if (el.target !== '_blank') {
        el.target = '_top';
      }
    });

    postOneGoogleBarLoaded();
  });
} else {
  const {gbar} = window as Window & Gbar;
  if (gbar && gbar.a) {
    postOneGoogleBarLoaded();
  }
}

export {};
