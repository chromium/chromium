// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const chooseDirButton = document.querySelector('#choose-directory');
const resultDialog = document.querySelector('#result-dialog');

chooseDirButton.addEventListener('click', async event => {
  resultDialog.showModal();
});
