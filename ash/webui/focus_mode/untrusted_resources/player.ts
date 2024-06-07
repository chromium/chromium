// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TRUSTED_ORIGIN = 'chrome://focus-mode-media';

let player: HTMLAudioElement|null = null;

interface Track {
  // The URL of the audio data.
  mediaUrl: string;
  // The URL of the track thumbnail.
  thumbnailUrl: string;
  // The title of the track.
  title: string;
  // The track's artist.
  artist: string;
}

function isTrack(a: any): a is Track {
  return a && typeof a == 'object' && typeof a.mediaUrl == 'string' &&
      typeof a.thumbnailUrl == 'string' && typeof a.title == 'string' &&
      typeof a.artist == 'string';
}

interface Command {
  cmd: string;
  arg: any;
}

function isCommand(a: any): a is Command {
  return a && typeof a == 'object' && typeof a.cmd == 'string';
}

function sendTrackRequest() {
  parent.postMessage({cmd: 'gettrack'}, TRUSTED_ORIGIN);
}

function loadTrack(track: Track) {
  if (player) {
    player.src = track.mediaUrl;
    navigator.mediaSession.metadata = new MediaMetadata({
      title: track.title,
      artist: track.artist,
      // TODO: Implement thumbnail handling. We apparently cannot simply use the
      // thumbnailUrl here. It seems that the parent frame will handle the
      // actual network load (presumably because we're working with `navigator`)
      // and since the parent isn't allowed to do network requests, the thing
      // will fail. A workaround that should be fairly easy to implement is to
      // load the thumbnail manually in this frame and convert the data to a
      // blob: or data: URL.
    });
  }
}

globalThis.addEventListener('load', () => {
  player = document.getElementById('player') as HTMLAudioElement;
  sendTrackRequest();
});

globalThis.addEventListener('message', (event: MessageEvent) => {
  if (event.origin != TRUSTED_ORIGIN) {
    return;
  }

  const data = event.data;
  if (isCommand(data)) {
    if (data.cmd == 'play' && isTrack(data.arg)) {
      loadTrack(data.arg);
    }
  }
});
