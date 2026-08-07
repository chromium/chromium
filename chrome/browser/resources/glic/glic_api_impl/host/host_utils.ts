// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InterfaceDef, PendingRemote, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

export interface HasMojoConnection {
  onConnectionError: {addListener: (l: Function) => number};
  $: {close(): void, bindNewPipeAndPassRemote(): unknown};
}
export interface HasPostMessagePipe {
  addCloseHandler(f: Function): void;
  close(): void;
}

// Automatically closes all pipes when one of them is closed.
export function linkPipeClosure(
    ...entries: Array<HasPostMessagePipe|HasMojoConnection>) {
  let activeEntries: Array<HasPostMessagePipe|HasMojoConnection>|null = entries;
  const destroy = () => {
    if (!activeEntries) {
      return;
    }
    for (const entry of activeEntries) {
      if ((entry as Partial<HasMojoConnection>).$) {
        (entry as HasMojoConnection).$.close();
      } else {
        (entry as HasPostMessagePipe).close();
      }
    }
    activeEntries = null;
  };

  for (const entry of entries) {
    if ((entry as Partial<HasMojoConnection>).$) {
      (entry as HasMojoConnection).onConnectionError.addListener(() => {
        destroy();
      });
    } else {
      (entry as HasPostMessagePipe).addCloseHandler(() => {
        destroy();
      });
    }
  }
}

// Automatically creates a postMessage remote from a PendingRemote, wraps it
// in a Mojo receiver using the provided constructors, links their lifecycles,
// and returns the bound Mojo receiver and Mojo remote.
export function createForwardingMojoRemote<
    MojoInterface, PostMessageInterface extends
        InterfaceDef, MojoReceiverType extends HasMojoConnection>(
    router: PostMessageRouter,
    postMessagePipe: PendingRemote<PostMessageInterface>,
    receiverConstructor: new (impl: MojoInterface) => MojoReceiverType,
    implConstructor: new (remote: PostMessageRemote<PostMessageInterface>) =>
        MojoInterface,
    ): {
  receiver: MojoReceiverType,
  remote: ReturnType<MojoReceiverType['$']['bindNewPipeAndPassRemote']>,
} {
  const pmRemote = router.newRemote(postMessagePipe);
  const mojoReceiver = new receiverConstructor(new implConstructor(pmRemote));
  const mojoRemote = mojoReceiver.$.bindNewPipeAndPassRemote() as
      ReturnType<MojoReceiverType['$']['bindNewPipeAndPassRemote']>;
  linkPipeClosure(pmRemote, mojoReceiver);
  return {receiver: mojoReceiver, remote: mojoRemote};
}
