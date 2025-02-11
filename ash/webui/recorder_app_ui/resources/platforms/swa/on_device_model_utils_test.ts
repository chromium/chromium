// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run unit test by `npx tsx on_device_model_utils_test.ts`

import {strict as assert} from 'node:assert';
import {describe, it} from 'node:test';

import {LanguageCode} from '../../core/soda/language_info.js';

import {
  isInvalidFormatResponse,
  parseResponse,
  trimRepeatedBulletPoints,
} from './on_device_model_utils.js';

describe('parseResponse', () => {
  it('should remove multiple \\n', () => {
    assert.equal(
      parseResponse('- point 1\n\n\n\n- point 2\n\n\n\n\n'),
      '- point 1\n- point 2',
    );
  });

  it(
    'should trim leading and tailing space',
    () => {
      assert.equal(parseResponse('  - point 1 '), '- point 1');
    },
  );

  it('should remove ▁', () => {
    assert.equal(parseResponse('- remove this▁'), '- remove this');
  });
});

describe('isInvalidFormatResponse', () => {
  [
    // invalid format
    {
      title: 'return true if response is empty',
      response: '',
      expectedBulletPoint: 0,
      result: true,
    }, {
      title: 'return true when response has no bullet point',
      response: 'one_word_only',
      expectedBulletPoint: 0,
      result: true,
    }, {
      title: 'return true when bullet points are less than expected',
      response: '- point 1\n- point 2\n',
      expectedBulletPoint: 3,
      result: true,
    }, {
      title: 'return true when bullet point does\'t start with -',
      response: '- point 1\npoint 2\n',
      expectedBulletPoint: 2,
      result: true,
    },
    // valid format
    {
      title: 'return false when contain one bullet point',
      response: '- point 1',
      expectedBulletPoint: 1,
      result: false,
    }, {
      title: 'return false when contain bullet points',
      response: '- point 1\n- point 2',
      expectedBulletPoint: 2,
      result: false,
    }, {
      title: 'return false when bullet point starts with spaces',
      response: '- point 1\n   - point 2',
      expectedBulletPoint: 2,
      result: false,
    }].map((testCase) => {
      it(`should ${testCase.title}`, () => {
        assert(
          isInvalidFormatResponse(
            testCase.response,
            testCase.expectedBulletPoint,
          ) === testCase.result,
        );
      });
    });
});

describe('trimRepeatedBulletPoints', () => {
  it('should trim 0-word bullet point', () => {
    assert.deepEqual(
      trimRepeatedBulletPoints(
        '- \n- some words',
        100,
        LanguageCode.EN_US,
        0.9,
      ),
      [
        '- some words',
      ],
    );
  });

  it(
    'should trim shorter repeated bullet point',
    () => {
      assert.deepEqual(
        trimRepeatedBulletPoints(
          '- same\n- same here', 100, LanguageCode.EN_US, 0.9,
        ),
        ['- same here'],
      );
    },
  );

  it(
    'should trim multiple repeated bullet points',
    () => {
      assert.deepEqual(
        trimRepeatedBulletPoints(
          '- same\n- same here\n- same here again',
          100,
          LanguageCode.EN_US,
          0.9,
        ),
        ['- same here again'],
      );
    },
  );

  it(
    'should trim multiple repeated groups',
    () => {
      assert.deepEqual(
        trimRepeatedBulletPoints(
          '- same1\n- same1 here\n- same2\n- same2 here',
          100,
          LanguageCode.EN_US,
          0.9,
        ),
        [
          '- same1 here',
          '- same2 here',
        ],
      );
    },
  );

  it('should trim too long bullet points', () => {
      assert.deepEqual(
        trimRepeatedBulletPoints(
          '- point 1\n- point 2 with long test',
          9,
          LanguageCode.EN_US,
          0.9,
        ),
        ['- point 1'],
      );
  });

  it('should not trim if no repetition', () => {
    assert.deepEqual(
      trimRepeatedBulletPoints(
        '- point 1\n- point 2',
        100,
        LanguageCode.EN_US,
        0.9,
      ),
      [
        '- point 1',
        '- point 2',
      ],
    );
  });
});
