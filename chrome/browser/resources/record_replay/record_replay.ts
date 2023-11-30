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

let browserProxy: BrowserProxy;

class BrowserProxy {
  private callbackRouter_: RecordReplayManagerCallbackRouter =
      new RecordReplayManagerCallbackRouter();
  private handler_: RecordReplayManagerHandlerRemote;

  constructor() {
    this.callbackRouter_.handleRecordingStateChanged.addListener(
        this.handleRecordingStateChanged.bind(this));

    this.handler_ = RecordReplayManagerHandler.getRemote();
    this.handler_.setManager(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
    this.handler_.apiKeyReceived("test_api_key");
  }

  private handleRecordingStateChanged(new_state: string) {
    console.error("[RUN-2886] Recording state changed to " + new_state);
  }
}

document.addEventListener("DOMContentLoaded", () => {
  console.error("[RUN-2886] DOMContentLoaded");
  browserProxy = new BrowserProxy();
});
