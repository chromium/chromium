// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kSerializedKeyIdeaTemplate[] = R"({
  "endOffset": %i,
  "startOffset": %i,
  "text": "%s"
})";

constexpr char kSerializedTranscriptTemplate[] = R"({
  "endOffset": %i,
  "groupId": %i,
  "startOffset": %i,
  "text": "%s",
  "hypothesisParts": %s
})";

constexpr char kSerializedHypothesisPartTemplate[] = R"({
  "text": %s,
  "offset": %i
})";

constexpr char kCompleteMetadataTemplateWithStatus[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "groupId": 1000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "text"
            ]
          }
        ],
        "startOffset": 1000,
        "text": "transcript text"
      },
      {
        "endOffset": 5000,
        "groupId": 3000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "2"
            ]
          }
        ],
        "startOffset": 3000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": %i,
    "version": 2,
    "tableOfContent": []
  })";

constexpr char kCompleteMetadataV2Template[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "text"
            ]
          }
        ],
        "startOffset": 1000,
        "groupId": 1000,
        "text": "transcript text"
      },
      {
        "endOffset": 5000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "2"
            ]
          }
        ],
        "startOffset": 3000,
        "groupId": 3000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": 1,
    "version": 2,
    "tableOfContent": []
  })";

constexpr char kCompleteMetadataV2WithDelimiterTemplate[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "▁transcript"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "▁text"
            ]
          }
        ],
        "startOffset": 1000,
        "groupId": 1000,
        "text": "transcript text"
      },
      {
        "endOffset": 5000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "▁transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "▁text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "▁2"
            ]
          }
        ],
        "startOffset": 3000,
        "groupId": 3000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": 1,
    "version": 2,
    "tableOfContent": []
  })";

constexpr char kCompleteMetadataV2MultipleSentenceTemplate[] = R"({
    "captions": [
      {
        "endOffset": 4000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Mr.",
              "mr."
            ]
          },
          {
            "offset": 1000,
            "text": [
              "X",
              "x"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 3000,
            "text": [
              "text?",
              "text"
            ]
          }
        ],
        "startOffset": 0,
        "groupId": 0,
        "text": "Mr. X Transcript text?"
      },
      {
        "endOffset": 6000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text!",
              "text"
            ]
          }
        ],
        "startOffset": 4000,
        "groupId": 0,
        "text": "Transcript text!"
      },
      {
        "endOffset": 8000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 6000,
        "groupId": 0,
        "text": "Transcript text."
      },

      {
        "endOffset": 12000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Mr.",
              "mr."
            ]
          },
          {
            "offset": 1000,
            "text": [
              "X",
              "x"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 3000,
            "text": [
              "text?",
              "text"
            ]
          }
        ],
        "startOffset": 8000,
        "groupId": 8000,
        "text": "Mr. X Transcript text?"
      },
      {
        "endOffset": 14000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text!",
              "text"
            ]
          }
        ],
        "startOffset": 12000,
        "groupId": 8000,
        "text": "Transcript text!"
      },
      {
        "endOffset": 16000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 14000,
        "groupId": 8000,
        "text": "Transcript text."
      },

      {
        "endOffset": 25000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text",
              "text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "2",
              "2"
            ]
          }
        ],
        "startOffset": 19000,
        "groupId": 19000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": 1,
    "version": 2,
    "tableOfContent": []
  })";

constexpr char kCompleteMetadataV2ChineseTemplate[] = R"({
  "captionLanguage": "zh",
  "captions": [
    {
      "endOffset": 56000,
      "groupId": 0,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "发",
            "发"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "明",
            "明"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "前",
            "前"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "口",
            "口"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "头",
            "头"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "知",
            "知"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "识",
            "识"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "在",
            "在"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "传",
            "传"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "播",
            "播"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "和",
            "和"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "积",
            "积"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "累",
            "累"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "中",
            "中"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "有",
            "有"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "明",
            "明"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "显",
            "显"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "缺",
            "缺"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "点，",
            "点，"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "原",
            "原"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "始",
            "始"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "人",
            "人"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "类",
            "类"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "使",
            "使"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "用",
            "用"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "了",
            "了"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "结",
            "结"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "绳、",
            "绳"
          ]
        },
        {
          "offset": 31000,
          "text": [
            "刻",
            "刻"
          ]
        },
        {
          "offset": 32000,
          "text": [
            "契、",
            "契"
          ]
        },
        {
          "offset": 33000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 34000,
          "text": [
            "画",
            "画"
          ]
        },
        {
          "offset": 35000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 36000,
          "text": [
            "方",
            "方"
          ]
        },
        {
          "offset": 37000,
          "text": [
            "法",
            "法"
          ]
        },
        {
          "offset": 38000,
          "text": [
            "辅",
            "辅"
          ]
        },
        {
          "offset": 39000,
          "text": [
            "助",
            "助"
          ]
        },
        {
          "offset": 40000,
          "text": [
            "记",
            "记"
          ]
        },
        {
          "offset": 41000,
          "text": [
            "事，",
            "事，"
          ]
        },
        {
          "offset": 42000,
          "text": [
            "后",
            "后"
          ]
        },
        {
          "offset": 43000,
          "text": [
            "来",
            "来"
          ]
        },
        {
          "offset": 44000,
          "text": [
            "用",
            "用"
          ]
        },
        {
          "offset": 45000,
          "text": [
            "特",
            "特"
          ]
        },
        {
          "offset": 46000,
          "text": [
            "征",
            "征"
          ]
        },
        {
          "offset": 47000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 48000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 49000,
          "text": [
            "来",
            "来"
          ]
        },
        {
          "offset": 50000,
          "text": [
            "简",
            "简"
          ]
        },
        {
          "offset": 51000,
          "text": [
            "化、",
            "化"
          ]
        },
        {
          "offset": 52000,
          "text": [
            "取",
            "取"
          ]
        },
        {
          "offset": 53000,
          "text": [
            "代",
            "代"
          ]
        },
        {
          "offset": 54000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 55000,
          "text": [
            "画。",
            "画"
          ]
        }
      ],
      "startOffset": 0,
      "text": "文字发明前的口头知识在传播和积累中有明显缺点，原始人类使用了结绳、刻契、图画的方法辅助记事，后来用特征图形来简化、取代图画。"
    },
    {
      "endOffset": 88000,
      "groupId": 0,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "当",
            "当"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "符",
            "符"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "号",
            "号"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "简",
            "简"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "化",
            "化"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "到",
            "到"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "一",
            "一"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "定",
            "定"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "程",
            "程"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "度，",
            "度，"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "并",
            "并"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "与",
            "与"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "语",
            "语"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "言",
            "言"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "特",
            "特"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "定",
            "定"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "对",
            "对"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "应",
            "应"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "时，",
            "时，"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "原",
            "原"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "始",
            "始"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "就",
            "就"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 31000,
          "text": [
            "了。",
            "了"
          ]
        }
      ],
      "startOffset": 56000,
      "text": "当图形符号简化到一定程度，并形成与语言的特定对应时，原始文字就形成了。"
    },
    {
      "endOffset": 119000,
      "groupId": 0,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "唐",
            "唐"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "兰",
            "兰"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "在",
            "在"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "《",
            "《"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "古",
            "古"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "学",
            "学"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "导",
            "导"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "论",
            "论"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "》",
            "》"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "中",
            "中"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "将",
            "将"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "古",
            "古"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "分",
            "分"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "殷",
            "殷"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "商",
            "商"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "西",
            "西"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "周",
            "周"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "六",
            "六"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "国",
            "国"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "秦",
            "秦"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "系",
            "系"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "四",
            "四"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "系。",
            "系"
          ]
        }
      ],
      "startOffset": 88000,
      "text": "唐兰在《古文字学导论》中将古文字分成殷商系、西周系、六国系、秦系四系。"
    },
    {
      "endOffset": 175000,
      "groupId": 119000,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "发",
            "发"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "明",
            "明"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "前",
            "前"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "口",
            "口"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "头",
            "头"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "知",
            "知"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "识",
            "识"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "在",
            "在"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "传",
            "传"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "播",
            "播"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "和",
            "和"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "积",
            "积"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "累",
            "累"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "中",
            "中"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "有",
            "有"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "明",
            "明"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "显",
            "显"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "缺",
            "缺"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "点，",
            "点，"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "原",
            "原"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "始",
            "始"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "人",
            "人"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "类",
            "类"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "使",
            "使"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "用",
            "用"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "了",
            "了"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "结",
            "结"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "绳、",
            "绳"
          ]
        },
        {
          "offset": 31000,
          "text": [
            "刻",
            "刻"
          ]
        },
        {
          "offset": 32000,
          "text": [
            "契、",
            "契"
          ]
        },
        {
          "offset": 33000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 34000,
          "text": [
            "画",
            "画"
          ]
        },
        {
          "offset": 35000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 36000,
          "text": [
            "方",
            "方"
          ]
        },
        {
          "offset": 37000,
          "text": [
            "法",
            "法"
          ]
        },
        {
          "offset": 38000,
          "text": [
            "辅",
            "辅"
          ]
        },
        {
          "offset": 39000,
          "text": [
            "助",
            "助"
          ]
        },
        {
          "offset": 40000,
          "text": [
            "记",
            "记"
          ]
        },
        {
          "offset": 41000,
          "text": [
            "事，",
            "事，"
          ]
        },
        {
          "offset": 42000,
          "text": [
            "后",
            "后"
          ]
        },
        {
          "offset": 43000,
          "text": [
            "来",
            "来"
          ]
        },
        {
          "offset": 44000,
          "text": [
            "用",
            "用"
          ]
        },
        {
          "offset": 45000,
          "text": [
            "特",
            "特"
          ]
        },
        {
          "offset": 46000,
          "text": [
            "征",
            "征"
          ]
        },
        {
          "offset": 47000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 48000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 49000,
          "text": [
            "来",
            "来"
          ]
        },
        {
          "offset": 50000,
          "text": [
            "简",
            "简"
          ]
        },
        {
          "offset": 51000,
          "text": [
            "化、",
            "化"
          ]
        },
        {
          "offset": 52000,
          "text": [
            "取",
            "取"
          ]
        },
        {
          "offset": 53000,
          "text": [
            "代",
            "代"
          ]
        },
        {
          "offset": 54000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 55000,
          "text": [
            "画。",
            "画"
          ]
        }
      ],
      "startOffset": 119000,
      "text": "文字发明前的口头知识在传播和积累中有明显缺点，原始人类使用了结绳、刻契、图画的方法辅助记事，后来用特征图形来简化、取代图画。"
    },
    {
      "endOffset": 207000,
      "groupId": 119000,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "当",
            "当"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "图",
            "图"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "符",
            "符"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "号",
            "号"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "简",
            "简"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "化",
            "化"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "到",
            "到"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "一",
            "一"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "定",
            "定"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "程",
            "程"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "度，",
            "度，"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "并",
            "并"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "与",
            "与"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "语",
            "语"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "言",
            "言"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "的",
            "的"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "特",
            "特"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "定",
            "定"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "对",
            "对"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "应",
            "应"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "时，",
            "时，"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "原",
            "原"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "始",
            "始"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "就",
            "就"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "形",
            "形"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 31000,
          "text": [
            "了。",
            "了"
          ]
        }
      ],
      "startOffset": 175000,
      "text": "当图形符号简化到一定程度，并形成与语言的特定对应时，原始文字就形成了。"
    },
    {
      "endOffset": 238000,
      "groupId": 119000,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "唐",
            "唐"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "兰",
            "兰"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "在",
            "在"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "《",
            "《"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "古",
            "古"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "学",
            "学"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "导",
            "导"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "论",
            "论"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "》",
            "》"
          ]
        },
        {
          "offset": 11000,
          "text": [
            "中",
            "中"
          ]
        },
        {
          "offset": 12000,
          "text": [
            "将",
            "将"
          ]
        },
        {
          "offset": 13000,
          "text": [
            "古",
            "古"
          ]
        },
        {
          "offset": 14000,
          "text": [
            "文",
            "文"
          ]
        },
        {
          "offset": 15000,
          "text": [
            "字",
            "字"
          ]
        },
        {
          "offset": 16000,
          "text": [
            "分",
            "分"
          ]
        },
        {
          "offset": 17000,
          "text": [
            "成",
            "成"
          ]
        },
        {
          "offset": 18000,
          "text": [
            "殷",
            "殷"
          ]
        },
        {
          "offset": 19000,
          "text": [
            "商",
            "商"
          ]
        },
        {
          "offset": 20000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 21000,
          "text": [
            "西",
            "西"
          ]
        },
        {
          "offset": 22000,
          "text": [
            "周",
            "周"
          ]
        },
        {
          "offset": 23000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 24000,
          "text": [
            "六",
            "六"
          ]
        },
        {
          "offset": 25000,
          "text": [
            "国",
            "国"
          ]
        },
        {
          "offset": 26000,
          "text": [
            "系、",
            "系"
          ]
        },
        {
          "offset": 27000,
          "text": [
            "秦",
            "秦"
          ]
        },
        {
          "offset": 28000,
          "text": [
            "系",
            "系"
          ]
        },
        {
          "offset": 29000,
          "text": [
            "四",
            "四"
          ]
        },
        {
          "offset": 30000,
          "text": [
            "系。",
            "系"
          ]
        }
      ],
      "startOffset": 207000,
      "text": "唐兰在《古文字学导论》中将古文字分成殷商系、西周系、六国系、秦系四系。"
    }
  ],
  "recognitionStatus": 1,
  "tableOfContent": [],
  "version": 2
})";

constexpr char kCompleteMetadataV2JapaneseWithLatinCharactersTemplate[] = R"({
  "captions": [
    {
      "endOffset": 11000,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "こ"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "れ"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "は、"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "3"
          ]
        },
        {
          "offset": 4000,
          "text": [
            "km"
          ]
        },
        {
          "offset": 5000,
          "text": [
            "などの"
          ]
        },
        {
          "offset": 6000,
          "text": [
            "数字を"
          ]
        },
        {
          "offset": 7000,
          "text": [
            "含むラ"
          ]
        },
        {
          "offset": 8000,
          "text": [
            "ンダム"
          ]
        },
        {
          "offset": 9000,
          "text": [
            "なテキ"
          ]
        },
        {
          "offset": 10000,
          "text": [
            "ストです。"
          ]
        }
      ],
      "startOffset": 0,
      "groupId": 0,
      "text": "これは、3 km などの数字を含むランダムなテキストです。"
    },
    {
      "endOffset": 15000,
      "hypothesisParts": [
        {
          "offset": 0,
          "text": [
            "これ"
          ]
        },
        {
          "offset": 1000,
          "text": [
            "も分"
          ]
        },
        {
          "offset": 2000,
          "text": [
            "割した"
          ]
        },
        {
          "offset": 3000,
          "text": [
            "い文です。"
          ]
        }
      ],
      "startOffset": 11000,
      "groupId": 0,
      "text": "これも分割したい文です。"
    }
  ],
  "captionLanguage": "ja",
  "recognitionStatus": 1,
  "version": 2,
  "tableOfContent": []
})";

void AssertSerializedString(const std::string& expected,
                            const std::string& actual) {
  std::optional<base::Value> expected_value = base::JSONReader::Read(expected);
  ASSERT_TRUE(expected_value);
  std::string expected_serialized_value;
  base::JSONWriter::Write(expected_value.value(), &expected_serialized_value);
  EXPECT_EQ(expected_serialized_value, actual);
}

std::string BuildKeyIdeaJson(int start_offset,
                             int end_offset,
                             const std::string& text) {
  return base::StringPrintf(kSerializedKeyIdeaTemplate, end_offset,
                            start_offset, text.c_str());
}

std::string BuildHypothesisParts(
    const media::HypothesisParts& hypothesis_parts) {
  std::stringstream ss;
  ss << "[";
  for (uint i = 0; i < hypothesis_parts.text.size(); i++) {
    ss << "\"" << hypothesis_parts.text[i] << "\"";
    if (i < hypothesis_parts.text.size() - 1) {
      ss << ", ";
    }
  }
  ss << "]";

  return base::StringPrintf(
      kSerializedHypothesisPartTemplate, ss.str().c_str(),
      int(hypothesis_parts.hypothesis_part_offset.InMilliseconds()));
}

std::string BuildHypothesisPartsList(
    const std::vector<media::HypothesisParts>& hypothesis_parts_vector) {
  std::stringstream ss;
  ss << "[";
  for (uint i = 0; i < hypothesis_parts_vector.size(); i++) {
    ss << BuildHypothesisParts(hypothesis_parts_vector[i]);
    if (i < hypothesis_parts_vector.size() - 1) {
      ss << ", ";
    }
  }
  ss << "]";
  return ss.str();
}

std::string BuildTranscriptJson(
    int start_offset,
    int group_id,
    int end_offset,
    const std::string& text,
    const std::vector<media::HypothesisParts>& hypothesis_part) {
  return base::StringPrintf(kSerializedTranscriptTemplate, end_offset, group_id,
                            start_offset, text.c_str(),
                            BuildHypothesisPartsList(hypothesis_part).c_str());
}

std::unique_ptr<ProjectorMetadata> populateMetadata(
    bool with_delimiters = false) {
  base::i18n::SetICUDefaultLocale("en_US");
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  metadata->SetCaptionLanguage("en");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  std::vector<media::HypothesisParts> first_transcript;
  first_transcript.emplace_back(
      std::vector<std::string>(
          {with_delimiters ? "▁transcript" : "transcript"}),
      base::Milliseconds(0));
  first_transcript.emplace_back(
      std::vector<std::string>({with_delimiters ? "▁text" : "text"}),
      base::Milliseconds(2000));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), 1000, "transcript text",
      std::move(first_transcript)));

  metadata->MarkKeyIdea();

  std::vector<media::HypothesisParts> second_transcript;
  second_transcript.emplace_back(
      std::vector<std::string>(
          {with_delimiters ? "▁transcript" : "transcript"}),
      base::Milliseconds(0));
  second_transcript.emplace_back(
      std::vector<std::string>({with_delimiters ? "▁text" : "text"}),
      base::Milliseconds(1000));
  second_transcript.emplace_back(
      std::vector<std::string>({with_delimiters ? "▁2" : "2"}),
      base::Milliseconds(1500));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(3000),
      /*end_time=*/base::Milliseconds(5000), 3000, "transcript text 2",
      std::move(second_transcript)));
  return metadata;
}

std::unique_ptr<ProjectorMetadata> populateMetadataWithSentences() {
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  base::i18n::SetICUDefaultLocale("en_US");
  metadata->SetCaptionLanguage("en");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  const std::vector<std::string> paragraph_words = {
      "Mr.",        "X",     "Transcript", "text?",
      "Transcript", "text!", "Transcript", "text."};
  const std::vector<std::string> noromalized_paragraph_words = {
      "mr.",        "x",    "transcript", "text",
      "transcript", "text", "transcript", "text"};
  std::string paragraph_text =
      "Mr. X Transcript text? Transcript text! Transcript text.";
  std::vector<media::HypothesisParts> paragraph_hypothesis_parts;
  for (uint i = 0; i < paragraph_words.size(); i++) {
    paragraph_hypothesis_parts.emplace_back(
        std::vector<std::string>(
            {paragraph_words[i], noromalized_paragraph_words[i]}),
        base::Milliseconds(i * 1000));
  }
  const base::TimeDelta paragraph_start_offset = base::Milliseconds(0);
  const base::TimeDelta paragraph_end_offset =
      base::Milliseconds(paragraph_words.size() * 1000);

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_start_offset, paragraph_end_offset,
      paragraph_start_offset.InMilliseconds(),
      base::JoinString(paragraph_words, " "), paragraph_hypothesis_parts));

  // Add another paragraph with the same text and length.
  // The group id for the new paragraph should be paragraph_end_offset,
  // start timestamp should be paragraph_end_offset + hypothesiePart offset.
  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_end_offset, paragraph_end_offset + paragraph_end_offset,
      paragraph_end_offset.InMilliseconds(), paragraph_text,
      paragraph_hypothesis_parts));

  metadata->MarkKeyIdea();

  std::vector<media::HypothesisParts> second_transcript;
  second_transcript.emplace_back(
      std::vector<std::string>({"transcript", "transcript"}),
      base::Milliseconds(0));
  second_transcript.emplace_back(std::vector<std::string>({"text", "text"}),
                                 base::Milliseconds(1000));
  second_transcript.emplace_back(std::vector<std::string>({"2", "2"}),
                                 base::Milliseconds(1500));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(19000),
      /*end_time=*/base::Milliseconds(25000), 9000, "transcript text 2",
      std::move(second_transcript)));
  return metadata;
}

std::unique_ptr<ProjectorMetadata> populateMetadataWithLanguageWithoutSpaces() {
  // Test on a language that does not use space.
  base::i18n::SetICUDefaultLocale("zh");
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  metadata->SetCaptionLanguage("zh");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  // "。" is the punctuation marking sentence end in Chinese, similar to "." in
  // English. The paragraph text comes from Wikipedia Chinese characters page in
  // Chinese language https://zh.wikipedia.org/wiki/%E6%B1%89%E5%AD%97
  std::string paragraph_text =
      "文字发明前的口头知识在传播和积累中有明显缺点，原始人类使用了结绳、刻契、"
      "图画的方法辅助记事，后来用特征图形来简化、取代图画。当图形符号简化到一定"
      "程度，并形成与语言的特定对应时，原始文字就形成了。唐兰在《古文字学导论》"
      "中将古文字分成殷商系、西周系、六国系、秦系四系。";
  const std::vector<std::string> paragraph_words = {
      "文",   "字",   "发", "明",   "前",   "的",   "口",   "头",   "知",
      "识",   "在",   "传", "播",   "和",   "积",   "累",   "中",   "有",
      "明",   "显",   "缺", "点，", "原",   "始",   "人",   "类",   "使",
      "用",   "了",   "结", "绳、", "刻",   "契、", "图",   "画",   "的",
      "方",   "法",   "辅", "助",   "记",   "事，", "后",   "来",   "用",
      "特",   "征",   "图", "形",   "来",   "简",   "化、", "取",   "代",
      "图",   "画。", "当", "图",   "形",   "符",   "号",   "简",   "化",
      "到",   "一",   "定", "程",   "度，", "并",   "形",   "成",   "与",
      "语",   "言",   "的", "特",   "定",   "对",   "应",   "时，", "原",
      "始",   "文",   "字", "就",   "形",   "成",   "了。", "唐",   "兰",
      "在",   "《",   "古", "文",   "字",   "学",   "导",   "论",   "》",
      "中",   "将",   "古", "文",   "字",   "分",   "成",   "殷",   "商",
      "系、", "西",   "周", "系、", "六",   "国",   "系、", "秦",   "系",
      "四",   "系。",
  };
  const std::vector<std::string> noromalized_paragraph_words = {
      "文", "字",   "发",   "明", "前", "的", "口", "头", "知",   "识", "在",
      "传", "播",   "和",   "积", "累", "中", "有", "明", "显",   "缺", "点，",
      "原", "始",   "人",   "类", "使", "用", "了", "结", "绳",   "刻", "契",
      "图", "画",   "的",   "方", "法", "辅", "助", "记", "事，", "后", "来",
      "用", "特",   "征",   "图", "形", "来", "简", "化", "取",   "代", "图",
      "画", "当",   "图",   "形", "符", "号", "简", "化", "到",   "一", "定",
      "程", "度，", "并",   "形", "成", "与", "语", "言", "的",   "特", "定",
      "对", "应",   "时，", "原", "始", "文", "字", "就", "形",   "成", "了",
      "唐", "兰",   "在",   "《", "古", "文", "字", "学", "导",   "论", "》",
      "中", "将",   "古",   "文", "字", "分", "成", "殷", "商",   "系", "西",
      "周", "系",   "六",   "国", "系", "秦", "系", "四", "系",

  };

  std::vector<media::HypothesisParts> paragraph_hypothesis_parts;
  for (uint i = 0; i < paragraph_words.size(); i++) {
    paragraph_hypothesis_parts.emplace_back(
        std::vector<std::string>(
            {paragraph_words[i], noromalized_paragraph_words[i]}),
        base::Milliseconds(i * 1000));
  }
  const base::TimeDelta paragraph_start_offset = base::Milliseconds(0);
  const base::TimeDelta paragraph_end_offset =
      base::Milliseconds(paragraph_words.size() * 1000);

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_start_offset, paragraph_end_offset,
      paragraph_start_offset.InMilliseconds(), paragraph_text,
      paragraph_hypothesis_parts));

  // Add another paragraph with the same text and length.
  // The group id for the new paragraph should be paragraph_end_offset,
  // start timestamp should be paragraph_end_offset + hypothesiePart offset.
  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_end_offset, paragraph_end_offset + paragraph_end_offset,
      paragraph_end_offset.InMilliseconds(), paragraph_text,
      paragraph_hypothesis_parts));
  return metadata;
}

std::unique_ptr<ProjectorMetadata> populateMetadataWithMixedCharacters() {
  base::i18n::SetICUDefaultLocale("ja");
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  metadata->SetCaptionLanguage("ja");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  std::string paragraph_text =
      "これは、3 km "
      "などの数字を含むランダムなテキストです。これも分割したい文です。";
  const std::vector<std::string> paragraph_words = {
      "こ",         "れ",     "は、",   "3",      "km",
      "などの",     "数字を", "含むラ", "ンダム", "なテキ",
      "ストです。", "これ",   "も分",   "割した", "い文です。",
  };

  std::vector<media::HypothesisParts> paragraph_hypothesis_parts;
  for (uint i = 0; i < paragraph_words.size(); i++) {
    paragraph_hypothesis_parts.emplace_back(
        std::vector<std::string>({paragraph_words[i]}),
        base::Milliseconds(i * 1000));
  }
  const base::TimeDelta paragraph_start_offset = base::Milliseconds(0);
  const base::TimeDelta paragraph_end_offset =
      base::Milliseconds(paragraph_words.size() * 1000);

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_start_offset, paragraph_end_offset,
      paragraph_start_offset.InMilliseconds(), paragraph_text,
      paragraph_hypothesis_parts));
  return metadata;
}

}  // namespace

class ProjectorKeyIdeaTest : public testing::Test {
 public:
  ProjectorKeyIdeaTest() = default;

  ProjectorKeyIdeaTest(const ProjectorKeyIdeaTest&) = delete;
  ProjectorKeyIdeaTest& operator=(const ProjectorKeyIdeaTest&) = delete;
};

TEST_F(ProjectorKeyIdeaTest, ToJson) {
  ProjectorKeyIdea key_idea(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000));

  std::string key_idea_str;
  base::JSONWriter::Write(key_idea.ToJson(), &key_idea_str);

  AssertSerializedString(BuildKeyIdeaJson(1000, 3000, std::string()),
                         key_idea_str);
}

TEST_F(ProjectorKeyIdeaTest, ToJsonWithText) {
  ProjectorKeyIdea key_idea(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), "Key idea text");

  std::string key_idea_str;
  base::JSONWriter::Write(key_idea.ToJson(), &key_idea_str);

  AssertSerializedString(BuildKeyIdeaJson(1000, 3000, "Key idea text"),
                         key_idea_str);
}

class ProjectorTranscriptTest : public testing::Test {
 public:
  ProjectorTranscriptTest() = default;

  ProjectorTranscriptTest(const ProjectorTranscriptTest&) = delete;
  ProjectorTranscriptTest& operator=(const ProjectorTranscriptTest&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorTranscriptTest, ToJson) {
  std::vector<media::HypothesisParts> hypothesis_parts;
  hypothesis_parts.emplace_back(std::vector<std::string>({"transcript"}),
                                base::Milliseconds(1000));
  hypothesis_parts.emplace_back(std::vector<std::string>({"text"}),
                                base::Milliseconds(2000));

  const auto expected_transcript = BuildTranscriptJson(
      1000, 1000, 3000, "transcript text", hypothesis_parts);

  ProjectorTranscript transcript(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), 1000, "transcript text",
      std::move(hypothesis_parts));

  std::string transcript_str;
  base::JSONWriter::Write(transcript.ToJson(), &transcript_str);

  AssertSerializedString(expected_transcript, transcript_str);
}

class ProjectorMetadataTest : public testing::Test {
 public:
  ProjectorMetadataTest() = default;

  ProjectorMetadataTest(const ProjectorMetadataTest&) = delete;
  ProjectorMetadataTest& operator=(const ProjectorMetadataTest&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorMetadataTest, Serialize) {
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kIncomplete);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplateWithStatus,
                         static_cast<int>(RecognitionStatus::kIncomplete)),
      metadata->Serialize());

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplateWithStatus,
                         static_cast<int>(RecognitionStatus::kComplete)),
      metadata->Serialize());

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kError);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplateWithStatus,
                         static_cast<int>(RecognitionStatus::kError)),
      metadata->Serialize());

  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
  // V2 feature flag not enabled, setting version number has no effort.
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplateWithStatus,
                         static_cast<int>(RecognitionStatus::kError)),
      metadata->Serialize());
}

class ProjectorMetadataTestV2 : public testing::Test {
 public:
  ProjectorMetadataTestV2() = default;

  ProjectorMetadataTestV2(const ProjectorMetadataTestV2&) = delete;
  ProjectorMetadataTestV2& operator=(const ProjectorMetadataTestV2&) = delete;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorMetadataTestV2, SerializeV2) {
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2Template, metadata->Serialize());
}

TEST_F(ProjectorMetadataTestV2, AddSingleSentenceTranscriptForV2) {
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2Template, metadata->Serialize());
}

TEST_F(ProjectorMetadataTestV2, AddMultiSentenceTranscriptForV2) {
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadataWithSentences();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  // There are 3 sentences in first and second paragraph transcript, 1 in third
  // making total count 3*2 + 1 = 7.
  EXPECT_EQ(metadata->GetTranscriptsCount(), 7ul);
  AssertSerializedString(kCompleteMetadataV2MultipleSentenceTemplate,
                         metadata->Serialize());
}

TEST_F(ProjectorMetadataTestV2, AddMultiSentenceTranscriptWithChinese) {
  std::unique_ptr<ProjectorMetadata> metadata =
      populateMetadataWithLanguageWithoutSpaces();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  // There are 3 sentences in each paragraph transcript, making total count 3*2
  // = 6.
  EXPECT_EQ(metadata->GetTranscriptsCount(), 6ul);
  AssertSerializedString(kCompleteMetadataV2ChineseTemplate,
                         metadata->Serialize());
}

TEST_F(ProjectorMetadataTestV2, RemoveDelimiter) {
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata(true);
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2WithDelimiterTemplate,
                         metadata->Serialize());
}

TEST_F(ProjectorMetadataTestV2, PreserveSpacingForMixedCharacters) {
  std::unique_ptr<ProjectorMetadata> metadata =
      populateMetadataWithMixedCharacters();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2JapaneseWithLatinCharactersTemplate,
                         metadata->Serialize());
}

}  // namespace ash
