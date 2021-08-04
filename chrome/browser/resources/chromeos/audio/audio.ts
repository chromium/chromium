// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioBroker} from './audio_broker.js';

function initialize() {
  const handler = AudioBroker.getInstance().handler;
  handler.getAudioDeviceInfo().then(({deviceName}) => {
    console.log('mock device name output: ' + deviceName);
  });
  console.log('welcome to the audio page.');
}

document.addEventListener('DOMContentLoaded', initialize);
