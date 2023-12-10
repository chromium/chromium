// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests screencast viewport size.');

  let lastImageWidth = 0;
  let lastImageHeight = 0;
  let colorChangeCount = 0;

  function setBkgrColor(bkgrColor) {
    session.evaluate(`document.body.style.backgroundColor = "${bkgrColor}"`);
  }

  function changeBkgrColor(image) {
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(image, 0, 0);
    const rgba = ctx.getImageData(0, 0, 1, 1).data;

    if (rgba[0] === 0 && rgba[3] === 255) {
      if (rgba[1] === 255 && rgba[2] === 0) {
        ++colorChangeCount;
        setBkgrColor('#0000ff');
      } else if (rgba[1] === 0 && rgba[2] === 255) {
        ++colorChangeCount;
        setBkgrColor('#00ff00');
      }
    } else {
      setBkgrColor('#00ff00');
    }
  }

  function saveImageSize(image) {
    if (lastImageWidth != image.naturalWidth ||
        lastImageHeight != image.naturalHeight) {
      lastImageWidth = image.naturalWidth;
      lastImageHeight = image.naturalHeight;
    }
  }

  async function loadPngAndChangeColor(pngBase64) {
    const image = new Image();

    await new Promise(resolve => {
      image.onload = resolve;
      image.src = `data:image/png;base64,${pngBase64}`;
    });

    saveImageSize(image);
    changeBkgrColor(image);
  }

  await dp.Page.enable();

  dp.Page.onScreencastFrame(async (data) => {
    const pngBase64 = data.params.data;
    await loadPngAndChangeColor(pngBase64);

    const sessionId = data.params.sessionId;
    await dp.Page.screencastFrameAck({sessionId});

    if (colorChangeCount > 4) {
      await dp.Page.stopScreencast();
      testRunner.log(`Image size: ${lastImageWidth} x ${lastImageHeight}`);
      testRunner.completeTest();
    }
  });

  dp.Page.bringToFront();
  dp.Emulation.setVisibleSize({width: 640, height: 480});
  dp.Page.startScreencast({format: 'png'});
})
