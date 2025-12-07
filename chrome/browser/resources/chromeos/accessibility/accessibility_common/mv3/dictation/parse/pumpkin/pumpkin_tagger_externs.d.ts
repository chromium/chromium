// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for PumpkinTagger. These are added on an as-needed
 * basis.
 */

declare namespace proto {
  export namespace speech {
    export namespace pumpkin {
      export interface ActionArgument {
        name: string;
        argumentType: string;
        userType: string;
        score: number;
        unnormalizedValue: string;
        value: any;
      }

      export interface HypothesisResult {
        actionExportName: string;
        actionName: string;
        actionArgumentList: ActionArgument[];
        score: number;
        taggedHypothesis: string;
      }

      export interface PumpkinTaggerResults {
        hypothesisList: HypothesisResult[];
      }
    }
  }
}

declare namespace speech {
  export namespace pumpkin {
    export namespace api {
      export namespace js {
        export interface PumpkinTagger {
          /**
           * Loads the PumpkinTagger from a PumpkinConfig proto binary file.
           * This proto file can be generated using
           * pumpkin/tools/build_binary_configs which converts an
           * pumpkin.config and directory of grammar .far files into a single
           * binary file.
           */
          initializeFromPumpkinConfig:
              (buffer: ArrayBuffer) => Promise<boolean>;
          /**
           * Loads an ActionFrame from an ActionSetConfig proto binary file.
           * This proto file can be generated using
           * pumpkin/tools/build_binary_configs which converts an
           * action.config and directory of .far files into a single binary
           * file.
           */
          loadActionFrame: (buffer: ArrayBuffer) => Promise<boolean>;
          /**
           * Cleans up C++ memory. Should be called before the tagger is
           * deconstructed.
           */
          cleanUp: () => void;
          /** Tags an input string and returns the results. */
          tagAndGetNBestHypotheses:
              (input: string,
               numResults: number) => proto.speech.pumpkin.PumpkinTaggerResults;
        }
      }
    }
  }
}
