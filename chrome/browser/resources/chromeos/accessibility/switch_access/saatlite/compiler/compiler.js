// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Main entry point for the SAATLite compiler. It handles reading the .saatl
 * in files, calling the parser, and writing the .js out files.
 */

const parse = require('./parser').parse;
const fs = require('fs');

const preamble = `// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../switch_access_e2e_test_base.js', '../../test_utility.js']);

/** Test fixture for the SAATLite generated tests. */
SwitchAccessSAATLiteTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
      await TestUtility.setup();
  }
};

`;

const args = process.argv.slice(2);
const testDir = args[0];
const outFile = args[1];

if (typeof testDir !== 'string' || typeof outFile !== 'string') {
  throw new Error(
      'Error: compiler needs two string arguments: ' +
      'the test dir and the out file.');
}

// Delete the output file if it already exists.
if (fs.existsSync(outFile)) {
  fs.unlinkSync(outFile);
}

const stream = fs.createWriteStream(outFile);
stream.on('error', console.error);
stream.on('open', () => {
  stream.write(preamble);

  // Read all the files in the tests/ directory.
  const filenames = fs.readdirSync(testDir);
  filenames.forEach(filename => {
    console.log('Compiling file: ', filename);
    const contents = fs.readFileSync(testDir + filename, {encoding: 'utf8'});
    stream.write(parse(contents).output + '\n');
  });

  stream.end();
});
