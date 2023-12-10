// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const connectToWebCam = async () => {
  const constraints = {video: true};
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
  const videoElement = document.getElementById('cameraStream');
  videoElement.srcObject = stream;
  // Send the stream to the background page.
  chrome.runtime.sendMessage({type: 'cameraStream', stream});
};

const button = document.createElement('button');
button.textContent = 'Click to start FaceGaze';
button.addEventListener('click', connectToWebCam);
document.body.appendChild(button);
