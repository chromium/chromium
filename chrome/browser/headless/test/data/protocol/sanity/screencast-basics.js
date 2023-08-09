// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests basic screencast functionality.');

  let seenGreen = 0;
  let seenBlue = 0;

  function setBkgrColor(bkgrColor) {
    session.evaluate(`document.body.style.backgroundColor = "${bkgrColor}"`);
  }

  async function loadPngAndCountPixelColor(pngBase64) {
    const image = new Image();

    await new Promise(resolve => {
      image.onload = resolve;
      image.src = `data:image/png;base64,${pngBase64}`;
    });

    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(image, 0, 0);
    const rgba = ctx.getImageData(0, 0, 1, 1).data;

    if (rgba[0] === 0 && rgba[3] === 255) {
      if (rgba[1] === 255 && rgba[2] === 0) {
        ++seenGreen;
        setBkgrColor('#0000ff');
      } else if (rgba[1] === 0 && rgba[2] === 255) {
        ++seenBlue;
        setBkgrColor('#00ff00');
      }
    } else {
      setBkgrColor('#00ff00');
    }
  }

  await dp.Page.enable();

  dp.Page.onScreencastFrame(async (data) => {
    const pngBase64 = data.params.data;
    await loadPngAndCountPixelColor(pngBase64);
    if (seenGreen > 2 && seenBlue > 2) {
      await dp.Page.stopScreencast();
      testRunner.log(`Seen both green and blue page backgrounds.`);
      testRunner.completeTest();
    }

    const sessionId = data.params.sessionId;
    await dp.Page.screencastFrameAck({sessionId});
  });

  dp.Page.bringToFront();

  dp.Page.startScreencast({format: 'png'});
})
