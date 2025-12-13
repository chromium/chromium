// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AbslStatusCode} from '//resources/mojo/mojo/public/mojom/base/absl_status.mojom-webui.js';

import type {PageHandlerInterface} from './data_sharing.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './data_sharing.mojom-webui.js';
import type {GroupAction, GroupActionProgress} from './data_sharing.mojom-webui.js';
import type {DataSharingSdkSitePreview} from './data_sharing_sdk_types.js';
import {Code} from './data_sharing_sdk_types.js';

export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler?: PageHandlerInterface;
  showUi(): void;
  closeUi(status: Code): void;
  makeTabGroupShared(tabGroupId: string, groupId: string, tokenSecret: string):
      Promise<string|undefined>;
  aboutToUnShareTabGroup(tabGroupId: string): void;
  onTabGroupUnShareComplete(tabGroupId: string): void;
  getShareLink(groupId: string, tokenSecret: string): Promise<string>;
  getTabGroupPreview(groupId: string, tokenSecret: string):
      Promise<DataSharingSdkSitePreview[]>;
  onGroupAction(action: GroupAction, progress: GroupActionProgress): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  closeUi(status: Code) {
    this.handler.closeUI(status);
  }

  aboutToUnShareTabGroup(tabGroupId: string) {
    this.handler.aboutToUnShareTabGroup(tabGroupId);
  }

  onTabGroupUnShareComplete(tabGroupId: string) {
    this.handler.onTabGroupUnShareComplete(tabGroupId);
  }

  makeTabGroupShared(tabGroupId: string, groupId: string, tokenSecret: string):
      Promise<string|undefined> {
    return this.handler.makeTabGroupShared(tabGroupId, groupId, tokenSecret)
        .then(res => res.url ? res.url.url : undefined);
  }

  getShareLink(groupId: string, tokenSecret: string): Promise<string> {
    return this.handler.getShareLink(groupId, tokenSecret)
        .then(res => res.url.url);
  }

  getTabGroupPreview(groupId: string, tokenSecret: string):
      Promise<DataSharingSdkSitePreview[]> {
    return new Promise((resolve) => {
      const previews: DataSharingSdkSitePreview[] = [];
      this.handler.getTabGroupPreview(groupId, tokenSecret).then((res) => {
        if (res.groupPreview.statusCode !== AbslStatusCode.kOk) {
          this.handler.closeUI(Code.UNKNOWN);
        }

        res.groupPreview.sharedTabs.map((sharedTab) => {
          previews.push({
            url: sharedTab.displayUrl,
            faviconUrl:
                this.getFaviconServiceUrl(sharedTab.faviconUrl.url).toString(),
          });
        });
        resolve(previews);
      });
    });
  }

  onGroupAction(action: GroupAction, progress: GroupActionProgress) {
    return this.handler.onGroupAction(action, progress);
  }

  // TODO(crbug.com/392965221): Use function from icon.ts instead.
  private getFaviconServiceUrl(pageUrl: string): URL {
    const url: URL = new URL('chrome-untrusted://favicon2');
    url.searchParams.set('size', '16');
    url.searchParams.set('scaleFactor', '1x');
    url.searchParams.set('allowGoogleServerFallback', '1');
    url.searchParams.set('pageUrl', pageUrl);
    return url;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy|null) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
