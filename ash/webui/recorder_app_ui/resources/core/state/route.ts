// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {signal} from '../reactive/signal.js';

interface RouteInfo<ParameterKey extends string> {
  path: string;
  parameterKeys: ParameterKey[];
}

type ExtractParameterKey<T> = T extends RouteInfo<infer K>? K : never;

function createRoute<ParameterKey extends string>(
  path: string,
  parameterKeys: ParameterKey[],
): RouteInfo<ParameterKey> {
  return {path, parameterKeys};
}

/**
 * Routes of all the pages.
 *
 * This only defines the route path and necessary parameters, and what component
 * to actual render is defined in pages/recorder-app.ts.
 */
const routes = {
  index: createRoute('/', []),
  playback: createRoute('/playback', ['id']),
  record: createRoute('/record', ['includeSystemAudio', 'micId']),
  dev: createRoute('/dev', []),
  test: createRoute('/test', []),
} as const;

type Routes = typeof routes;
type RouteNames = keyof Routes;

interface CurrentRouteInfo<Name extends RouteNames> {
  name: Name;
  parameters: Record<ExtractParameterKey<Routes[Name]>, string|null>;
}

type UnionCurrentRoute = {
  [K in RouteNames]: CurrentRouteInfo<K>;
}[RouteNames];

export const currentRoute = signal<UnionCurrentRoute|null>(null);

function extractCurrentRouteInfo(url: URL): UnionCurrentRoute|null {
  // We use hash based client side navigation, to avoid the following issue
  // for modern path based client side navigation in our use case:
  // * recorder_app_ui.cc needs to have all the paths that it should handle.
  // * When serving bundled output via cra.py bundle, many static hosting
  //   server (like x20) doesn't support path rewrite and doesn't work well
  //   with client side navigation.
  // * The route below needs to handle when the bundled output is hosted on a
  //   subpath.
  //
  // TODO(pihsun): Since changing hash won't trigger page refresh, we
  // probably can simplify some of the logic in installRouter.
  const routeInHash = new URL(
    url.hash.slice(1),
    // Note that the origin part is not used and we only use the path and
    // search, but URL constructor requires a base URL if the first argument
    // is just a path.
    document.location.origin,
  );
  const path = routeInHash.pathname;
  const search = new URLSearchParams(routeInHash.search);

  for (const [name, info] of Object.entries(routes)) {
    if (path !== info.path) {
      continue;
    }
    const parameters: Record<string, string|null> = {};
    for (const key of info.parameterKeys) {
      parameters[key] = search.get(key);
    }
    // TypeScript can't deduce the type of name and the dependent parameters.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    return {name, parameters} as UnionCurrentRoute;
  }
  return null;
}

function updateRoute() {
  const url = new URL(window.location.href);
  currentRoute.value = extractCurrentRouteInfo(url);
}

function navigateToImpl(path: string) {
  window.history.pushState({}, '', `#${path}`);
  updateRoute();
}

/**
 * Navigates to the given path by name.
 *
 * Note that this should only be used for pages under Recorder App, and not
 * external link. Since we do client side navigation via URL hash (see the
 * reason in pages/recorder-app.ts), the path is put into URL hash.
 */
export function navigateTo<Name extends RouteNames>(
  name: Name,
  parameters: Partial<Record<ExtractParameterKey<Routes[Name]>, string>> = {},
): void {
  const {path} = routes[name];
  // TypeScript can't deduce that parameters is a sub-type of Record<string,
  // string>.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  const params = new URLSearchParams(parameters as Record<string, string>);
  navigateToImpl(`${path}?${params.toString()}`);
}

/**
 * Installs handler that intercept click on <a> to do client side navigation.
 */
export function installRouter(): void {
  document.body.addEventListener('click', (e) => {
    if (e.defaultPrevented || e.button !== 0 || e.altKey || e.ctrlKey ||
        e.shiftKey || e.metaKey) {
      return;
    }

    const anchor = e.composedPath().find((el): el is HTMLAnchorElement => {
      return el instanceof Node && el.nodeName === 'A';
    });
    if (anchor === undefined || anchor.target !== '' ||
        anchor.download !== '' || anchor.rel === 'external') {
      return;
    }

    const href = anchor.href;
    let url: URL;
    try {
      url = new URL(href);
      if (url.origin !== window.location.origin) {
        return;
      }
    } catch (_e) {
      // Error when parsing the anchor href, ignore it.
      return;
    }

    e.preventDefault();
    if (href !== window.location.href) {
      navigateToImpl(url.pathname);
    }
  });

  window.addEventListener('popstate', () => {
    updateRoute();
  });
  updateRoute();
}
