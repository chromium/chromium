// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function dismiss() {
  window.location.hash = '#dismiss';
}

function initialize() {
  const btn = document.getElementById('btn')!;
  btn.addEventListener('click', dismiss);
  let timeoutId = setTimeout(function() {
    window.close();
  }, 200);
  addEventListener('message', function() {
    clearTimeout(timeoutId);
    const iframe = document.getElementById('motd')!;
    iframe.hidden = false;
    const placeholder = document.getElementById('placeholder')!;
    placeholder.hidden = true;
  });
}

document.addEventListener('DOMContentLoaded', initialize);
