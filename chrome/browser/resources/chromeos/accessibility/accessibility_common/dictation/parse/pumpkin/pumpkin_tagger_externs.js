// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for PumpkinTagger. These are added on an as-needed
 * basis.
 */

const proto = {};

/** @const */
proto.speech = {};

/** @const */
proto.speech.pumpkin = {};

/** @const */
proto.speech.pumpkin.ActionArgument = {};

/**
 * @typedef {{
 *   name: string,
 *   argumentType: string,
 *   userType: string,
 *   score: number,
 *   unnormalizedValue: string,
 * }}
 */
proto.speech.pumpkin.ActionArgument.ObjectFormat = {};

/** @const */
proto.speech.pumpkin.HypothesisResult = {};

/**
 * @typedef {{
 *   actionExportName: string,
 *   actionName: string,
 *   actionArgumentList:
 *       Array<proto.speech.pumpkin.ActionArgument.ObjectFormat>, score: number,
 *   taggedHypothesis: string,
 * }}
 */
proto.speech.pumpkin.HypothesisResult.ObjectFormat = {};

/** @const */
proto.speech.pumpkin.PumpkinTaggerResults = {};

/**
 * @typedef {{
 *   hypothesisList: Array<proto.speech.pumpkin.HypothesisResult.ObjectFormat>,
 * }}
 */
proto.speech.pumpkin.PumpkinTaggerResults.ObjectFormat = {};

const speech = {};

/** @const */
speech.pumpkin = {};

/** @const */
speech.pumpkin.api = {};

/** @const */
speech.pumpkin.api.js = {};

/** @const */
speech.pumpkin.api.js.PumpkinTagger = {};

/** @constructor */
speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger = function() {};

/**
 * Loads the PumpkinTagger from a PumpkinConfig proto binary file.
 * This proto file can be generated using pumpkin/tools/build_binary_configs
 * which converts an pumpkin.config and directory of grammar .far files into a
 * single binary file.
 * @param {!ArrayBuffer} buffer The pumpkin config binary file contents.
 * @return {!Promise<boolean>}
 */
speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger.prototype
    .initializeFromPumpkinConfig = async function(buffer) {};

/**
 * Loads an ActionFrame from an ActionSetConfig proto binary file.
 * This proto file can be generated using pumpkin/tools/build_binary_configs
 * which converts an action.config and directory of .far files into a single
 * binary file.
 * @param {!ArrayBuffer} buffer The action set config binary file contents.
 * @return {!Promise<boolean>}
 */
speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger.prototype.loadActionFrame =
    async function(buffer) {};

/**
 * Cleans up C++ memory. Should be called before the tagger is
 * deconstructed.
 */
speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger.prototype.cleanUp =
    function() {};

/**
 * Tags an input string and returns the results.
 * @param {string} input The string to tag.
 * @param {number} numResults The maximum number of results.
 * @return {?proto.speech.pumpkin.PumpkinTaggerResults.ObjectFormat}
 */
speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger.prototype
    .tagAndGetNBestHypotheses = function(input, numResults) {};
