// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.readAnything API */

declare namespace chrome {
  export namespace readAnything {
    /////////////////////////////////////////////////////////////////////
    // Implemented in read_anything_app_controller.cc and consumed by ts.
    /////////////////////////////////////////////////////////////////////

    // The root AXNodeID of the tree to be displayed.
    let rootId: number;

    // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
    let fontName: string;
    let fontSize: number;
    let foregroundColor: number;
    let backgroundColor: number;
    let lineSpacing: number;
    let letterSpacing: number;

    // Returns a list of AXNodeIDs corresponding to the unignored children of
    // the AXNode for the provided AXNodeID. If there is a selection contained
    // in this node, only returns children which are partially or entirely
    // contained within the selection.
    function getChildren(nodeId: number): number[];

    // Returns the HTML tag of the AXNode for the provided AXNodeID.
    function getHtmlTag(nodeId: number): string;

    // Returns the language of the AXNode for the provided AXNodeID.
    function getLanguage(nodeId: number): string;

    // Returns the text content of the AXNode for the provided AXNodeID. If a
    // selection begins or ends in this node, truncates the text to only return
    // the selected text.
    function getTextContent(nodeId: number): string;

    // Returns the text direction of the AXNode for the provided AXNodeID.
    function getTextDirection(nodeId: number): string;

    // Returns the url of the AXNode for the provided AXNodeID.
    function getUrl(nodeId: number): string;

    // Connects to the browser process. Called by ts when the read anything
    // element is added to the document.
    function onConnected(): void;

    function onLinkClicked(nodeId: number): void;

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

    // Set the theme. Used by tests only.
    function setThemeForTesting(
        fontName: string, fontSize: number, foregroundColor: number,
        backgroundColor: number, lineSpacing: number,
        letterSpacing: number): void;

    ////////////////////////////////////////////////////////////////
    // Implemented in read_anything/app.ts and called by native c++.
    ////////////////////////////////////////////////////////////////

    // Ping that an AXTree has been distilled for the active tab's render frame
    // and is available to consume.
    function updateContent(): void;

    // Ping that the theme choices of the user have been changed using the
    // toolbar and are ready to consume.
    function updateTheme(): void;
  }
}
