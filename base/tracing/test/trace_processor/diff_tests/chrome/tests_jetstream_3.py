#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite

class ChromeJetStream3Stdlib(TestSuite):
  def test_jetstream_3_measure(self):
    return DiffTestBlueprint(
        trace=DataPath('jetstream_3.pftrace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.jetstream_3;

        SELECT
          name,
          top_level_name,
          iteration,
          subtest,
          dur
        FROM chrome_jetstream_3_measure
        ORDER BY name, iteration, subtest;
        """,
        out=Csv("""
        "name","top_level_name","iteration","subtest","dur"
        "3d-cube-SP","Sunspider",0,"First",16066000
        "3d-cube-SP","Sunspider",1,"Worst",8198000
        "3d-cube-SP","Sunspider",2,"Average",6570000
        "3d-cube-SP","Sunspider",3,"Worst",6909000
        "3d-cube-SP","Sunspider",4,"Average",6549000
        "3d-cube-SP","Sunspider",5,"Average",6604000
        "3d-cube-SP","Sunspider",6,"Worst",6721000
        "3d-cube-SP","Sunspider",7,"Average",6588000
        "3d-cube-SP","Sunspider",8,"Worst",6783000
        "3d-cube-SP","Sunspider",9,"Average",6499000
        "3d-raytrace-SP","Sunspider",0,"First",11646000
        "3d-raytrace-SP","Sunspider",1,"Worst",7495000
        "3d-raytrace-SP","Sunspider",2,"Worst",5214000
        "3d-raytrace-SP","Sunspider",3,"Average",4318000
        "3d-raytrace-SP","Sunspider",4,"Average",4367000
        "3d-raytrace-SP","Sunspider",5,"Worst",5694000
        "3d-raytrace-SP","Sunspider",6,"Worst",4724000
        "3d-raytrace-SP","Sunspider",7,"Average",3665000
        "3d-raytrace-SP","Sunspider",8,"Average",4194000
        "3d-raytrace-SP","Sunspider",9,"Average",3708000
        "Air","Air",0,"First",30374000
        "Air","Air",1,"Worst",7187000
        "Air","Air",2,"Worst",6350000
        "Air","Air",3,"Worst",5728000
        "Air","Air",4,"Worst",5278000
        "Air","Air",5,"Average",4106000
        "Air","Air",6,"Average",3859000
        "Air","Air",7,"Average",3926000
        "Air","Air",8,"Average",4018000
        "Air","Air",9,"Average",4206000
        "WSL","WSL",0,"WSL-mainRun",3677349000
        "WSL","WSL",0,"WSL-stdlib",540369000
        "base64-SP","Sunspider",0,"First",6408000
        "base64-SP","Sunspider",1,"Average",4507000
        "base64-SP","Sunspider",2,"Worst",5394000
        "base64-SP","Sunspider",3,"Worst",4980000
        "base64-SP","Sunspider",4,"Worst",6148000
        "base64-SP","Sunspider",5,"Worst",4791000
        "base64-SP","Sunspider",6,"Average",4372000
        "base64-SP","Sunspider",7,"Average",4015000
        "base64-SP","Sunspider",8,"Average",4237000
        "base64-SP","Sunspider",9,"Average",3996000
        "crypto-aes-SP","Sunspider",0,"First",8870000
        "crypto-aes-SP","Sunspider",1,"Worst",4952000
        "crypto-aes-SP","Sunspider",2,"Worst",4891000
        "crypto-aes-SP","Sunspider",3,"Worst",4772000
        "crypto-aes-SP","Sunspider",4,"Average",3765000
        "crypto-aes-SP","Sunspider",5,"Average",3715000
        "crypto-aes-SP","Sunspider",6,"Average",4072000
        "crypto-aes-SP","Sunspider",7,"Average",4039000
        "crypto-aes-SP","Sunspider",8,"Average",3994000
        "crypto-aes-SP","Sunspider",9,"Worst",4617000
        "crypto-md5-SP","Sunspider",0,"First",13394000
        "crypto-md5-SP","Sunspider",1,"Average",8375000
        "crypto-md5-SP","Sunspider",2,"Average",8773000
        "crypto-md5-SP","Sunspider",3,"Worst",10184000
        "crypto-md5-SP","Sunspider",4,"Worst",9174000
        "crypto-md5-SP","Sunspider",5,"Worst",9083000
        "crypto-md5-SP","Sunspider",6,"Worst",9278000
        "crypto-md5-SP","Sunspider",7,"Average",8472000
        "crypto-md5-SP","Sunspider",8,"Average",2559000
        "crypto-md5-SP","Sunspider",9,"Average",2557000
        "crypto-sha1-SP","Sunspider",0,"First",14932000
        "crypto-sha1-SP","Sunspider",1,"Worst",7611000
        "crypto-sha1-SP","Sunspider",2,"Average",6826000
        "crypto-sha1-SP","Sunspider",3,"Average",6816000
        "crypto-sha1-SP","Sunspider",4,"Worst",7123000
        "crypto-sha1-SP","Sunspider",5,"Average",6810000
        "crypto-sha1-SP","Sunspider",6,"Average",6842000
        "crypto-sha1-SP","Sunspider",7,"Average",6861000
        "crypto-sha1-SP","Sunspider",8,"Worst",6898000
        "crypto-sha1-SP","Sunspider",9,"Worst",8559000
        "date-format-tofte-SP","Sunspider",0,"First",7643000
        "date-format-tofte-SP","Sunspider",1,"Worst",6172000
        "date-format-tofte-SP","Sunspider",2,"Average",5422000
        "date-format-tofte-SP","Sunspider",3,"Worst",5793000
        "date-format-tofte-SP","Sunspider",4,"Average",5503000
        "date-format-tofte-SP","Sunspider",5,"Average",5498000
        "date-format-tofte-SP","Sunspider",6,"Worst",6192000
        "date-format-tofte-SP","Sunspider",7,"Average",5556000
        "date-format-tofte-SP","Sunspider",8,"Worst",5649000
        "date-format-tofte-SP","Sunspider",9,"Average",5644000
        "date-format-xparb-SP","Sunspider",0,"First",8026000
        "date-format-xparb-SP","Sunspider",1,"Worst",7532000
        "date-format-xparb-SP","Sunspider",2,"Worst",7223000
        "date-format-xparb-SP","Sunspider",3,"Average",7048000
        "date-format-xparb-SP","Sunspider",4,"Worst",7301000
        "date-format-xparb-SP","Sunspider",5,"Average",7073000
        "date-format-xparb-SP","Sunspider",6,"Worst",7251000
        "date-format-xparb-SP","Sunspider",7,"Average",7055000
        "date-format-xparb-SP","Sunspider",8,"Average",7086000
        "date-format-xparb-SP","Sunspider",9,"Average",7207000
        "n-body-SP","Sunspider",0,"First",5031000
        "n-body-SP","Sunspider",1,"Worst",3309000
        "n-body-SP","Sunspider",2,"Worst",3388000
        "n-body-SP","Sunspider",3,"Average",3086000
        "n-body-SP","Sunspider",4,"Average",3060000
        "n-body-SP","Sunspider",5,"Average",3056000
        "n-body-SP","Sunspider",6,"Average",3040000
        "n-body-SP","Sunspider",7,"Worst",3102000
        "n-body-SP","Sunspider",8,"Worst",3092000
        "n-body-SP","Sunspider",9,"Average",3059000
        "regex-dna-SP","Sunspider",0,"First",9365000
        "regex-dna-SP","Sunspider",1,"Worst",7718000
        "regex-dna-SP","Sunspider",2,"Average",7703000
        "regex-dna-SP","Sunspider",3,"Average",7671000
        "regex-dna-SP","Sunspider",4,"Average",7708000
        "regex-dna-SP","Sunspider",5,"Average",7705000
        "regex-dna-SP","Sunspider",6,"Worst",7765000
        "regex-dna-SP","Sunspider",7,"Worst",7793000
        "regex-dna-SP","Sunspider",8,"Worst",7889000
        "regex-dna-SP","Sunspider",9,"Average",7670000
        "string-unpack-code-SP","Sunspider",0,"First",7764000
        "string-unpack-code-SP","Sunspider",1,"Worst",4127000
        "string-unpack-code-SP","Sunspider",2,"Worst",4559000
        "string-unpack-code-SP","Sunspider",3,"Worst",4378000
        "string-unpack-code-SP","Sunspider",4,"Worst",4247000
        "string-unpack-code-SP","Sunspider",5,"Average",4048000
        "string-unpack-code-SP","Sunspider",6,"Average",4091000
        "string-unpack-code-SP","Sunspider",7,"Average",4076000
        "string-unpack-code-SP","Sunspider",8,"Average",4066000
        "string-unpack-code-SP","Sunspider",9,"Average",4026000
        "tagcloud-SP","Sunspider",0,"First",15777000
        "tagcloud-SP","Sunspider",1,"Worst",10022000
        "tagcloud-SP","Sunspider",2,"Worst",9492000
        "tagcloud-SP","Sunspider",3,"Average",9405000
        "tagcloud-SP","Sunspider",4,"Worst",10486000
        "tagcloud-SP","Sunspider",5,"Average",8852000
        "tagcloud-SP","Sunspider",6,"Average",8675000
        "tagcloud-SP","Sunspider",7,"Average",9201000
        "tagcloud-SP","Sunspider",8,"Average",9163000
        "tagcloud-SP","Sunspider",9,"Worst",9932000
        "typescript-octane","typescript-octane",0,"First",298729000
        "typescript-octane","typescript-octane",1,"Worst",156434000
        "typescript-octane","typescript-octane",2,"Average",131600000
        "typescript-octane","typescript-octane",3,"Average",128643000
        "typescript-octane","typescript-octane",4,"Average",116439000
        "typescript-octane","typescript-octane",5,"Average",127861000
        "typescript-octane","typescript-octane",6,"Worst",139550000
        "typescript-octane","typescript-octane",7,"Average",120516000
        "typescript-octane","typescript-octane",8,"Average",119588000
        "typescript-octane","typescript-octane",9,"Average",129313000
        """))

  def test_jetstream_3_score(self):
    return DiffTestBlueprint(
        trace=DataPath('jetstream_3.pftrace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.jetstream_3;

        SELECT
          top_level_name,
          format('%.5f', score) as score
        FROM chrome_jetstream_3_benchmark_score
        ORDER BY top_level_name;
        """,
        out=Csv("""
        "top_level_name","score"
        "Air","513.20932"
        "Sunspider","706.90280"
        "WSL","3.54697"
        "typescript-octane","27.91412"
        """))

  def test_jetstream_3_total_score(self):
    return DiffTestBlueprint(
        trace=DataPath('jetstream_3.pftrace.gz'),
        query="""
        INCLUDE PERFETTO MODULE chrome.jetstream_3;

        SELECT
          format('%.5f', chrome_jetstream_3_score()) as score;
        """,
        out=Csv("""
        "score"
        "77.41656"
        """))
