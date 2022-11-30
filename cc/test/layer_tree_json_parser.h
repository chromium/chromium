// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_JSON_PARSER_H_
#define CC_TEST_LAYER_TREE_JSON_PARSER_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace cc {

class ContentLayerClient;
class Layer;

scoped_refptr<Layer> ParseTreeFromJson(std::string json,
                                       ContentLayerClient* content_client);

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_JSON_PARSER_H_
