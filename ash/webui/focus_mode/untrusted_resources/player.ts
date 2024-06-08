// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TRUSTED_ORIGIN = 'chrome://focus-mode-media';

interface Track {
  // The URL of the audio data.
  mediaUrl: string;
  // The track thumbnail in the form of a data URL.
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

function getPlayerElement(): HTMLAudioElement {
  return document.getElementById('player') as HTMLAudioElement;
}

function loadTrack(track: Track) {
  const p = getPlayerElement();
  p.src = track.mediaUrl;

  const metadata: any = {
    title: track.title,
    artist: track.artist,
  };
  if (track.thumbnailUrl) {
    metadata.artwork = [{src: track.thumbnailUrl}];
  }
  navigator.mediaSession.metadata = new MediaMetadata(metadata);
}

globalThis.addEventListener('load', () => {
  getPlayerElement().addEventListener('ended', () => {
    sendTrackRequest();
  });

  // Registering this makes the "next track" button show up in the media
  // controls. We do not support going to the previous track.
  navigator.mediaSession.setActionHandler('nexttrack', () => {
    sendTrackRequest();
  });

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
