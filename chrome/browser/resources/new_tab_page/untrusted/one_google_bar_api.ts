// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

interface Task {
  fnName: string;
  args: any[];
}

export interface OneGoogleBarApi {
  setForegroundLight: (enabled: boolean) => void;
  trackDarkModeChanges: () => Promise<void>;
  processTaskQueue: () => Promise<void>;
}

export function createOneGoogleBarApi(abp: boolean): OneGoogleBarApi {
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
      queuedTask = {fnName, args};
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
  let queuedTask: Task|null = null;

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

    /* Process any pending OGB API call that may have been queued before the
     * OGB was ready to accept calls.
     */
    processTaskQueue: async () => {
      if (!queuedTask) {
        return;
      }

      await callAsyncBarApi(queuedTask.fnName, ...queuedTask.args);
      queuedTask = null;
    },
  };
}
