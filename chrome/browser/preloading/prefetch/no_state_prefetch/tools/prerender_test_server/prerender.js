// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function add_link(url, rel) {
  var link = document.getElementById(rel) || document.createElement('link');
  link.id = rel;
  link.rel = rel;
  link.href = url;
  document.body.appendChild(link);
}

function prerender_url() {
  var anchor = document.getElementById('anchor');
  anchor.innerText = '';
  anchor.href = '';
  var progress = document.getElementById('progress');
  progress.innerText = '';
  var input_url = document.getElementById('url');
  var url = input_url.value;
  if (!url)
    return false;
  if (!url.indexOf('http') == 0)
    url = 'http://' + url;
  // Set the input url to the url we're actually prerendering.
  input_url.value = url;
  add_link(url, 'prerender');

  window.setTimeout(function() {
    console.log('loaded');
    set_progress("Click to navigate to prerendered page: ");
    anchor.href = url;
    anchor.innerText = url;
  }, 0);
  console.log('prerendering: ' + url);
  return false;
}

function set_progress(progress_text) {
  var progress = document.getElementById('progress');
  progress.innerText = progress_text;
}
