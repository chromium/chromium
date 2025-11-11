// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {FreAppController} from '/fre/fre_app_controller.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {type PageInterface, type PanelStateKind, ProfileReadyState} from './glic.mojom-webui.js';
import {GlicAppController} from './glic_app_controller.js';

export enum AppView {
  GLIC,
  FRE
}

/**
 * This class manages the main view of the Glic WebUI, switching between the
 * First Run Experience (FRE) and the main GLIC application. It handles
 * the outcome of the FRE and delegates browser interactions to controllers.
 */
export class AppRouter implements PageInterface {
  private glicController: GlicAppController|undefined;
  private freAppController: FreAppController|undefined;
  private glicContainer: HTMLElement;
  private freContainer: HTMLElement;
  private browserProxy: BrowserProxyImpl;
  private currentView: AppView|undefined;
  private currentPanelStateKind: PanelStateKind|undefined;

  constructor() {
    this.glicContainer = getRequiredElement('glic-app-container');
    this.freContainer = getRequiredElement('fre-app-container');
    this.browserProxy = new BrowserProxyImpl(this);
    const shouldShowFre = loadTimeData.getBoolean('shouldShowFre');
    if (shouldShowFre) {
      this.switchToView(AppView.FRE);
    } else {
      this.switchToView(AppView.GLIC);
    }
  }

  switchToView(view: AppView): void {
    if (this.currentView === view) {
      return;
    }
    this.glicContainer.hidden = true;
    this.freContainer.hidden = true;
    const previousView = this.currentView;
    this.currentView = view;

    switch (this.currentView) {
      case AppView.GLIC:
        if (!this.glicController) {
          this.glicController = new GlicAppController(this.browserProxy);
          if (this.currentPanelStateKind !== undefined) {
            this.glicController.updatePageState(this.currentPanelStateKind);
          }
          this.freAppController?.destroyWebview();
          this.freAppController = undefined;
        }
        this.glicContainer.hidden = false;
        break;
      case AppView.FRE:
        if (previousView === AppView.GLIC) {
          throw new Error('Invalid view transition to FRE from GLIC');
        }
        if (!this.freAppController) {
          this.freAppController = new FreAppController({
            partitionString: 'persist:glicpart',
            shouldSizeForDialog: false,
            onClose: this.close.bind(this),
          });
        }
        this.freContainer.hidden = false;
        break;
    }
  }

  intentToShow() {
    this.glicController?.intentToShow();
  }

  setProfileReadyState(state: ProfileReadyState) {
    // If the view is currently FRE, transition to GLIC once in a ready state.
    if (this.currentView === AppView.FRE &&
        state === ProfileReadyState.kReady) {
      this.switchToView(AppView.GLIC);
    }
    this.glicController?.setProfileReadyState(state);
  }

  updatePageState(panelStateKind: PanelStateKind) {
    this.currentPanelStateKind = panelStateKind;
    this.glicController?.updatePageState(panelStateKind);
  }

  close(): void {
    this.browserProxy.handler.closePanel();
  }

  reload(): void {
    this.glicController?.reload();
  }

  showDebug(): void {
    this.glicController?.showDebug();
  }
}
