// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './icons.js';
import './shared_styles.js';

import {addWebUIListener, sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */ const AudioNodeType = {
  HEADPHONE: 'HEADPHONE',
  MIC: 'MIC',
  USB: 'USB',
  BLUETOOTH: 'BLUETOOTH',
  HDMI: 'HDMI',
  INTERNAL_SPEAKER: 'INTERNAL_SPEAKER',
  INTERNAL_MIC: 'INTERNAL_MIC',
  KEYBOARD_MIC: 'KEYBOARD_MIC',
  AOKR: 'AOKR',
  POST_MIX_LOOPBACK: 'POST_MIX_LOOPBACK',
  POST_DSP_LOOPBACK: 'POST_DSP_LOOPBACK',
  OTHER: 'OTHER',
};

/**
 * An audio node. Based on the struct AudioNode found in audio_node.h.
 * @constructor
 * @suppress {checkTypes}
 */
const AudioNode = function() {
  // Whether node will input or output audio.
  this.isInput = false;

  // Node ID. Set to 3000 because predefined output and input
  // nodes use 10000's and 20000's respectively and |nodeCount| will append it.
  this.id = '3000';

  // Display name of the node. When this is empty, cras will automatically
  // use |this.deviceName| as the display name.
  this.name = '';

  // The text label of the selected node name.
  this.deviceName = 'New Device';

  // Based on the AudioNodeType enum.
  this.type = AudioNodeType.OTHER;

  // Whether the node is active or not.
  this.active = false;

  // The time the node was plugged in (in seconds).
  this.pluggedTime = 0;
};

Polymer({
  is: 'audio-settings',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * An AudioNode which is currently being edited.
     * @type {?AudioNode}
     */
    currentEditableObject: {
      type: Object,
      value: null,
    },

    /**
     * The index of the audio node which is currently being edited.
     * This is initially set to -1 (i.e. no node selected) becuase no devices
     * have been copied.
     */
    currentEditIndex: {
      type: Number,
      value() {
        return -1;
      },
    },

    /**
     * A counter that will auto increment everytime a new node is added
     * or copied and used to set a new id. This allows the |AudioNode.id|
     * to allows be unique.
     */
    nodeCount: {
      type: Number,
      value() {
        return 0;
      },
    },

    /**
     * A set of audio nodes.
     * @type !Array<!AudioNode>
     */
    nodes: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * A set of options for the possible audio node types.
     * AudioNodeType |type| is based on the AudioType emumation.
     * @type {!Array<!{name: string, type: string}>}
     */
    nodeTypeOptions: {
      type: Array,
      value() {
        return [
          {name: 'Headphones', type: AudioNodeType.HEADPHONE},
          {name: 'Mic', type: AudioNodeType.MIC},
          {name: 'Usb', type: AudioNodeType.USB},
          {name: 'Bluetooth', type: AudioNodeType.BLUETOOTH},
          {name: 'HDMI', type: AudioNodeType.HDMI},
          {name: 'Internal Speaker', type: AudioNodeType.INTERNAL_SPEAKER},
          {name: 'Internal Mic', type: AudioNodeType.INTERNAL_MIC},
          {name: 'Keyboard Mic', type: AudioNodeType.KEYBOARD_MIC},
          {name: 'Aokr', type: AudioNodeType.AOKR},
          {name: 'Post Mix Loopback', type: AudioNodeType.POST_MIX_LOOPBACK},
          {name: 'Post Dsp Loopback', type: AudioNodeType.POST_DSP_LOOPBACK},
          {name: 'Other', type: AudioNodeType.OTHER},
        ];
      },
    },
  },

  ready() {
    addWebUIListener('audioNodesUpdated', this.updateAudioNodes_.bind(this));
    chrome.send('requestAudioNodes');
  },

  /**
   * Adds a new node with default settings to the list of nodes.
   */
  appendNewNode() {
    const newNode = new AudioNode();
    newNode.id += this.nodeCount;
    this.nodeCount++;
    this.push('nodes', newNode);
  },

  /**
   * This adds or modifies an audio node to the AudioNodeList.
   * @param {{model: {index: number}}} e Event with a model containing
   *     the index in |nodes| to add.
   */
  insertAudioNode(e) {
    // Create a new audio node and add all the properties from |nodes[i]|.
    const info = this.nodes[e.model.index];
    chrome.send('insertAudioNode', [info]);
  },

  /**
   * This adds/modifies the audio node |nodes[currentEditIndex]| to/from the
   * AudioNodeList.
   */
  insertEditedAudioNode() {
    // Insert a new node or update an existing node using all the properties
    // in |node|.
    const node = this.nodes[this.currentEditIndex];
    chrome.send('insertAudioNode', [node]);
    this.$.editDialog.close();
  },

  /**
   * Removes the audio node with id |id|.
   * @param {{model: {index: number}}} e Event with a model containing
   *     the index in |nodes| to remove.
   */
  removeAudioNode(e) {
    const info = this.nodes[e.model.index];
    chrome.send('removeAudioNode', [info.id]);
  },

  /**
   * Called on "copy" button from the device list clicked. Creates a copy of
   * the selected node.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  copyDevice(event) {
    // Create a shallow copy of the selected device.
    const newNode = new AudioNode();
    Object.assign(newNode, this.nodes[event.model.index]);
    newNode.name += ' (Copy)';
    newNode.deviceName += ' (Copy)';
    newNode.id += this.nodeCount;
    this.nodeCount++;

    this.push('nodes', newNode);
  },

  /**
   * Shows a dialog to edit the selected node's properties.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  showEditDialog(event) {
    const index = event.model.index;
    this.currentEditIndex = index;
    this.currentEditableObject = this.nodes[index];
    this.$.editDialog.showModal();
  },

  /**
   * Called by the WebUI which provides a list of nodes.
   * @param {!Array<!AudioNode>} nodeList A list of audio nodes.
   * @private
   */
  updateAudioNodes_(nodeList) {
    /** @type {!Array<!AudioNode>} */ const newNodeList = [];
    for (let i = 0; i < nodeList.length; ++i) {
      // Create a new audio node and add all the properties from |nodeList[i]|.
      const node = new AudioNode();
      Object.assign(node, nodeList[i]);
      newNodeList.push(node);
    }
    this.nodes = newNodeList;
  },
});
