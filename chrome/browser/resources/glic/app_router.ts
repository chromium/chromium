// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {ProfileReadyState, ZoomAction} from './glic.mojom-webui.js';
import {PanelStateKind} from './glic.mojom-webui.js';
import {GlicAppController} from './glic_app_controller.js';

export enum AppView {
  GLIC,
}

/**
 * This class manages the main view of the Glic WebUI, switching between the
 * First Run Experience (FRE) and the main GLIC application. It handles
 * the outcome of the FRE and delegates browser interactions to controllers.
 */
export class AppRouter {
  private glicController: GlicAppController|undefined;
  private glicContainer: HTMLElement;
  private browserProxy: BrowserProxyImpl;
  private currentView: AppView|undefined;
  private currentPanelStateKind: PanelStateKind|undefined;
  private instanceId: string|undefined;

  constructor() {
    this.glicContainer = getRequiredElement('glic-app-container');
    this.browserProxy = new BrowserProxyImpl();
    this.browserProxy.pageCallbackRouter.intentToShow.addListener(
        this.intentToShow_.bind(this),
    );
    this.browserProxy.pageCallbackRouter.updatePageState.addListener(
        this.updatePageState_.bind(this),
    );
    this.browserProxy.pageCallbackRouter.zoom.addListener(
        this.zoom_.bind(this),
    );
    this.browserProxy.instanceId.subscribe(this.setInstanceId_.bind(this));
    // TODO(crbug.com/454120908): Remove this method after WebContents warming
    // is rolled out.
    this.browserProxy.pageCallbackRouter.setProfileReadyState.addListener(
        this.setProfileReadyState_.bind(this),
    );
    this.browserProxy.preloadPageCallbackRouter.setProfileReadyState
        .addListener(
            this.setProfileReadyState_.bind(this),
        );
    this.switchToView(AppView.GLIC);
  }

  switchToView(view: AppView): void {
    if (this.currentView === view) {
      return;
    }
    this.glicContainer.hidden = true;
    this.currentView = view;

    switch (this.currentView) {
      case AppView.GLIC:
        if (!this.glicController) {
          this.glicController = new GlicAppController(this.browserProxy);
          if (this.currentPanelStateKind !== undefined) {
            this.glicController.updatePageState(this.currentPanelStateKind);
          }
        }
        this.glicContainer.hidden = false;
        break;
      default:
        assertNotReachedCase(this.currentView);
    }
  }

  private intentToShow_() {
    this.glicController?.intentToShow();
  }

  private setProfileReadyState_(state: ProfileReadyState) {
    this.glicController?.setProfileReadyState(state);
  }

  private updatePageState_(panelStateKind: PanelStateKind) {
    this.currentPanelStateKind = panelStateKind;
    this.glicController?.updatePageState(panelStateKind);
    this.updateUrl_();
  }

  private setInstanceId_(instanceId: string) {
    this.instanceId = instanceId;
    this.updateUrl_();
  }

  private updateUrl_() {
    try {
      const url = new URL(window.location.href);
      if (this.instanceId) {
        url.searchParams.set('instance', this.instanceId);
      }
      if (this.currentPanelStateKind !== undefined) {
        url.searchParams.set(
            'state',
            this.currentPanelStateKind !== PanelStateKind.kHidden ? 'Open' :
                                                                    'Closed');
      }
      window.history.replaceState({}, '', url.toString());
    } catch (e) {
      // Fallback in case window.location is unexpected.
    }
  }

  private zoom_(zoomAction: ZoomAction) {
    this.glicController?.zoom(zoomAction);
  }

  close(): void {
    this.browserProxy.pageHandler.closePanel();
  }

  reload(): void {
    this.glicController?.reload();
  }

  showDebug(): void {
    this.glicController?.showDebug();
  }
}
