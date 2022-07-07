// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.readAnything API */

declare namespace chrome {
  export namespace readAnything {
    /////////////////////////////////////////////////////////////////////
    // Implemented in read_anything_app_controller.cc and consumed by ts.
    /////////////////////////////////////////////////////////////////////

    // A list of AXNodeIDs corresponding to the content node IDs identified by
    // the AXTree distillation process.
    let contentNodeIds: number[];

    // A font name, defined in ReadAnythingFontModel.
    let fontName: string;

    let fontSize: number;

    // Returns a list of AXNodeIDs corresponding to the unignored children of
    // the AXNode for the provided AXNodeID.
    function getChildren(nodeId: number): number[];

    // Returns the heading level of the AXNode for the provided AXNodeID.
    function getHeadingLevel(nodeId: number): number;

    // Returns the text content of the AXNode for the provided AXNodeID.
    function getTextContent(nodeId: number): string;

    // Returns the url of the AXNode for the provided AXNodeID.
    function getUrl(nodeId: number): string;

    // Returns whether the AXNode for the provided AXNodeID is a heading.
    function isHeading(nodeId: number): boolean;

    // Returns whether the AXNode for the provided AXNodeID is a link,
    // represented by the anchor tag in HTML.
    function isLink(nodeId: number): boolean;

    // Returns whether the AXNode for the provided AXNodeID is a paragraph.
    function isParagraph(nodeId: number): boolean;

    // Returns whether the AXNode for the provided AXNodeID is a static text.
    function isStaticText(nodeId: number): boolean;

    // Connects to the browser process. Called by ts when the read anything
    // element is added to the document.
    function onConnected(): void;

    // Set the content. Used by tests only.
    // SnapshotLite is a data structure which resembles an AXTreeUpdate. E.g.:
    //   const axTree = {
    //     rootId: 1,
    //     nodes: [
    //       {
    //         id: 1,
    //         role: 'rootWebArea',
    //         childIds: [2],
    //       },
    //       {
    //         id: 2,
    //         role: 'staticText',
    //         name: 'Some text.',
    //       },
    //     ],
    //   };
    function setContentForTesting(
        snapshotLite: Object, contentNodeIds: number[]): void;

    // Set the font name. Used by tests only.
    function setFontNameForTesting(newFontName: string): void;

    ////////////////////////////////////////////////////////////////
    // Implemented in read_anything/app.ts and called by native c++.
    ////////////////////////////////////////////////////////////////

    // Ping that an AXTree has been distilled for the active tab's render frame
    // and is available to consume.
    function updateContent(): void;

    // Ping that the font name has been changed in the ReadAnythingToolbar and
    // is available to consume.
    function updateFontName(): void;

    // Ping that the font size has been changed for the distilled contents and
    // is available to consume.
    function updateFontSize(): void;
  }
}
