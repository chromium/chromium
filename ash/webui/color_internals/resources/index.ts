// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const chooseColorInput =
    document.querySelector<HTMLInputElement>('#choose-color')!;


chooseColorInput.addEventListener('change', async () => {
  console.log(chooseColorInput.value);
});
