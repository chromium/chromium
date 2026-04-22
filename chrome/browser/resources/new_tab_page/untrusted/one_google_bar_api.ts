// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface Bar {
  setDarkMode(matches: boolean): void;
  setBackgroundColor(bgColor: string): void;
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
  args: unknown[];
}

export interface OneGoogleBarApi {
  setDarkMode: (enabled: boolean) => void;
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
        ['setBackgroundColor', 'pd'],
        ['setDarkMode', 'pp'],
      ],
    } as Definition,
  ].reduce((topLevelApi, def) => {
    topLevelApi[def.name] = def.fns.reduce((apiPart, [name, fnName]) => {
      apiPart[name] = callApi.bind(null, def.apiName, fnName);
      return apiPart;
    }, {} as IndexableApi);
    return topLevelApi;
  }, {} as Record<string, IndexableApi>) as unknown as {bar: Bar};

  const asyncBar: AsyncBar = {
    setDarkMode: callAsyncBarApi.bind(null, 'pp'),
  };

  let queuedTask: Task|null = null;

  return {
    /**
     * Updates the foreground on the OneGoogleBar to provide contrast against
     * the background.
     */
    setDarkMode: (enabled: boolean) => {
      if (abp) {
        asyncBar.setDarkMode(enabled);
      } else {
        api.bar.setDarkMode(enabled);
      }
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
