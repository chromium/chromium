// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'env' : {
    'browser' : true,
    'es6' : true,
  },
  'extends': '../../../ui/webui/resources/tools/eslint_import_type.config.js',
  'rules' : {
    'eqeqeq' : ['error', 'always', {'null' : 'ignore'}],
  },
};
