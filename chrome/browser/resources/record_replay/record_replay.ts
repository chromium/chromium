// normal browser code should work, such as:
//
// const opened_window = window.open("");
// 
// function log(msg: string) {
//     console.log(msg);
//     if (opened_window) {
//         opened_window.document.body.textContent += msg;
//     }
// }
// 
// setInterval(() => log("hello again\n"), 1000);

// we should also be able to add mojo bindings or use `chrome.send()`
// and other things mentioned in webui_explainer.md.

//import "./auth.js";
// console.error("in record_replay.ts after importing auth.ts");

/*
import {
  RecordReplayManagerCallbackRouter,
  RecordReplayManagerHandler,
} from './record_replay.mojom-webui.js';
*/

import {
  RecordReplayManagerCallbackRouter,
  RecordReplayManagerHandler,
  RecordReplayManagerHandlerRemote,
} from './record_replay_manager.mojom-webui.js';
import {
  setRecordReplayInterface
} from './record_replay_api.js';
import './auth.js';

let browserProxy: BrowserProxy;

class BrowserProxy {
  private callbackRouter_: RecordReplayManagerCallbackRouter =
      new RecordReplayManagerCallbackRouter();
  private handler_: RecordReplayManagerHandlerRemote;
  private onSignInButtonClickedCallback_: (() => void) | null = null;

  constructor() {
    this.callbackRouter_.handleSignInButtonClicked.addListener(
      this.handleSignInButtonClicked.bind(this));

    this.handler_ = RecordReplayManagerHandler.getRemote();
    this.handler_.setManager(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  public async getEnv(key: string): Promise<string | null> {
    return (await this.handler_.getEnv(key)).value;
  }

  public async getBuildId(): Promise<string> {
    return (await this.handler_.getBuildId()).buildId
  }
  public async getReplayUserToken(): Promise<string | null> {
    return (await this.handler_.getReplayUserToken()).token;
  }
  public setReplayUserToken(token: string | null): void {
    this.handler_.setReplayUserToken(token);
  }
  public async getReplayRefreshToken(): Promise<string | null> {
    return (await this.handler_.getReplayRefreshToken()).token;
  }
  public setReplayRefreshToken(token: string | null): void {
    this.handler_.setReplayRefreshToken(token);
  }
  public showAuthenticationError(message: string): void {
    this.handler_.showAuthenticationError(message);
  }

  public onSignInButtonClicked(callback: () => void): void {
    this.onSignInButtonClickedCallback_ = callback;
  }

  public handleSignInButtonClicked(): void {
    if (this.onSignInButtonClickedCallback_) {
      this.onSignInButtonClickedCallback_();
    }
  }

  public openExternalBrowser(url: string): Promise<void> {
    this.handler_.openExternalBrowser(url);
    return Promise.resolve();
  }
}

document.addEventListener("DOMContentLoaded", () => {
  browserProxy = new BrowserProxy();
  setRecordReplayInterface(browserProxy);

  /*
   * Test code to trigger an unconditional sign-in after a delay.
   *
  // Click the sign-in button after a delay (test)
  setTimeout(() => {
    browserProxy.handleSignInButtonClicked();
  }, 1000);
  */
});
