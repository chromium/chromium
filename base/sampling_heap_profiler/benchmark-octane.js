// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// To benchmark a specific version of Chrome set the CHROME_PATH environment
// variable, e.g.:
// $ CHROME_PATH=~/chromium/src/out/Release/chrome node benchmark-octane.js

const puppeteer = require('puppeteer');

async function runOctane(samplingRate) {
  const args = ['--enable-devtools-experiments'];
  if (samplingRate)
    args.push(`--memlog=all`, `--memlog-sampling-rate=${samplingRate}`);
  while (true) {
    let browser;
    try {
      browser = await puppeteer.launch({
          executablePath: process.env.CHROME_PATH, args, headless: true});
      const page = await browser.newPage();
      await page.goto('https://chromium.github.io/octane/');
      await page.waitForSelector('#run-octane');  // Just in case.
      await page.click('#run-octane');

      const scoreDiv = await page.waitForSelector('#main-banner:only-child',
          {timeout: 120000});
      const scoreText = await page.evaluate(e => e.innerText, scoreDiv);
      const match = /Score:\s*(\d+)/.exec(scoreText);
      if (match.length < 2)
        continue;
      return parseInt(match[1]);
    } finally {
      if (browser)
        await browser.close();
    }
  }
}

async function makeRuns(rates) {
  const scores = [];
  for (const rate of rates)
    scores.push(await runOctane(rate));
  console.log(scores.join('\t'));
}

async function main() {
  console.log(`Using ${process.env.CHROME_PATH || puppeteer.executablePath()}`);
  const rates = [0];
  for (let rate = 8; rate <= 2048; rate *= 2)
    rates.push(rate);
  console.log('Rates [KB]:');
  console.log(rates.join('\t'));
  console.log('='.repeat(rates.length * 8));
  for (let i = 0; i < 100; ++i)
    await makeRuns(rates);
}

main();
