// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onDocumentLoaded() {
  // Find out the video, image, and caption urls from the url params.
  const urlParams = new URLSearchParams(window.location.search);
  video!.src = urlParams.get('video_url') || '';
  video!.poster = urlParams.get('poster_url') || '';
  track!.src = urlParams.get('caption_url') || '';
  video!.play();
}

function onVideoEnded() {
  // Resize the poster.
  video!.classList.add('video-ended');
  video!.controls = false;
}

const video = document.querySelector('video');
const track = document.querySelector('track');
if (!video || !track) {
  throw new Error('Failed to find video or track');
}
video.addEventListener('ended', onVideoEnded);
document.addEventListener('DOMContentLoaded', onDocumentLoaded);
