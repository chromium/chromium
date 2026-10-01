// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {mojo} from '//resources/mojo/mojo/public/js/bindings.js';
import {GenericAssociatedInterfacePendingReceiver} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {GenericAssociatedInterfacePendingReceiverEndpoint, GenericPendingAssociatedReceiver} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';

// The parts of a generated mojom remote (e.g. `FooRemote`) the host uses.
export interface InlineFulfillmentRemote {
  $: {
    associateAndPassReceiver():
        mojo.internal.interfaceSupport.PendingReceiver<unknown>,
    close(): void,
  };
}

// What every inline fulfillment module exports.
export interface InlineFulfillmentModule {
  // Returns the card, and an unbound remote the card talks to the browser
  // through. The host binds `remote` to the browser, and closes it when the
  // card is removed so the browser can stop any work for it.
  create(): {
    element: HTMLElement,
    interfaceName: string,
    remote: InlineFulfillmentRemote,
  };
}

export type InlineFulfillmentLoader = () => Promise<InlineFulfillmentModule>;

// Maps `InlineFulfillment.resource_name` to its module. Names not listed here
// are ignored.
// TODO(liuwilliam): Import latency-sensitive cards such as Explain statically,
// so the click doesn't wait on a module load before the browser gets the
// channel.
const LOADERS = new Map<string, InlineFulfillmentLoader>();

function toGenericChannel(
    interfaceName: string,
    receiver: mojo.internal.interfaceSupport.PendingReceiver<unknown>):
    GenericPendingAssociatedReceiver {
  const endpoint = receiver.handle as unknown as
      GenericAssociatedInterfacePendingReceiverEndpoint;
  return {
    interfaceName,
    receiver: new GenericAssociatedInterfacePendingReceiver(endpoint),
  };
}

// Shows the card for an inline fulfillment action.
export class InlineFulfillmentHostElement extends HTMLElement {
  static get is() {
    return 'glic-inline-fulfillment-host';
  }

  loaders: Map<string, InlineFulfillmentLoader> = LOADERS;

  // Bumped on every show() and clear(), so a load that finishes late is
  // dropped.
  private generation_ = 0;

  // The remote of the card that is showing.
  private remote_: InlineFulfillmentRemote|null = null;

  // Loads the module for `resourceName`, passes its channel to `send`, then
  // shows its card in place of the current one. Does nothing if the name is
  // unknown, the load or the card failed, or a newer show() or clear() ran
  // while loading.
  async show(
      resourceName: string,
      send: (channel: GenericPendingAssociatedReceiver) => void) {
    const generation = ++this.generation_;
    const load = this.loaders.get(resourceName);
    if (!load) {
      return;
    }
    let module: InlineFulfillmentModule;
    try {
      module = await load();
    } catch {
      // Treated the same as an unknown name.
      return;
    }
    if (generation !== this.generation_) {
      return;
    }
    let card: ReturnType<InlineFulfillmentModule['create']>|undefined;
    try {
      card = module.create();
      // Must be sent first: until the receiver is sent, the card's remote has
      // no pipe, and any call the card makes when it's added would fail.
      send(toGenericChannel(
          card.interfaceName, card.remote.$.associateAndPassReceiver()));
    } catch {
      // if create() or send() throw, treated the exception the same as a failed
      // load.
      card?.remote.$.close();
      return;
    }
    this.removeCard_();
    this.remote_ = card.remote;
    this.appendChild(card.element);
  }

  // Removes the card and drops any load in flight.
  clear() {
    this.generation_++;
    this.removeCard_();
  }

  // Removes the card and drops the card's remote.
  private removeCard_() {
    this.remote_?.$.close();
    this.remote_ = null;
    this.replaceChildren();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-inline-fulfillment-host': InlineFulfillmentHostElement;
  }
}

customElements.define(
    InlineFulfillmentHostElement.is, InlineFulfillmentHostElement);
