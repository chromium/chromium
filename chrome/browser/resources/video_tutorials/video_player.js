// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

function onDocumentLoaded() {
  // Find out the video, image, and caption urls from the url params.
  const urlParams = new URLSearchParams(window.location.search);
  video.src = urlParams.get('video_url');
  video.poster = urlParams.get('poster_url');
  track.src = urlParams.get('caption_url');
  video.play();
}

function onVideoEnded() {
  // Resize the poster.
  video.style.classList.add('video-ended');
  video.controls = false;
}

const video = $('video');
const track = $('track');
video.addEventListener('ended', onVideoEnded);
document.addEventListener('DOMContentLoaded', onDocumentLoaded);
