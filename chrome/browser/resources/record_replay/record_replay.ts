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

import * as RR from './record_replay_manager.mojom-webui.js';
console.error("Imported RR Api = " + [...Object.keys(RR)].join(", "));
