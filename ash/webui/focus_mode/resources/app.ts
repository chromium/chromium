// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MediaClientInterface, MediaClientReceiver, TrackDefinition, TrackProvider, TrackProviderInterface} from './focus_mode.mojom-webui.js';

const UNTRUSTED_ORIGIN = 'chrome-untrusted://focus-mode-player';

// Implemented here, used by Ash to directly start a different track.
let clientInstance: MediaClientImpl|null = null;

// Implemented by Ash, provides the tracks that we will play.
let providerInstance: TrackProviderInterface|null = null;

function getProvider(): TrackProviderInterface {
  if (!providerInstance) {
    providerInstance = TrackProvider.getRemote();
  }
  return providerInstance;
}

// Post a track play request to the iframe.
function postPlayRequest(track: TrackDefinition) {
  const child = document.getElementById('child') as HTMLIFrameElement;
  if (child.contentWindow) {
    child.contentWindow.postMessage(
        {
          cmd: 'play',
          arg: {
            mediaUrl: track.mediaUrl.url,
            thumbnailUrl: track.thumbnailUrl.url,
            title: track.title,
            artist: track.artist,
          },
        },
        UNTRUSTED_ORIGIN);
  }
}

class MediaClientImpl implements MediaClientInterface {
  static init() {
    if (!clientInstance) {
      clientInstance = new MediaClientImpl();
    }
  }

  static shutdown() {
    if (clientInstance) {
      clientInstance.receiver_.$.close();
      clientInstance = null;
    }
  }

  startPlay(track: TrackDefinition): void {
    postPlayRequest(track);
  }

  private receiver_: MediaClientReceiver = this.initReceiver_();

  private initReceiver_(): MediaClientReceiver {
    const receiver = new MediaClientReceiver(this);
    getProvider().setMediaClient(receiver.$.bindNewPipeAndPassRemote());
    return receiver;
  }
}

globalThis.addEventListener('load', async () => {
  MediaClientImpl.init();
});

globalThis.addEventListener('message', async (event: MessageEvent) => {
  if (event.origin != UNTRUSTED_ORIGIN) {
    return;
  }

  const data = event.data;
  if (data && typeof data == 'object' && typeof data.cmd == 'string') {
    if (data.cmd == 'gettrack') {
      const result = await getProvider().getTrack();
      postPlayRequest(result.track);
    }
  }
});
