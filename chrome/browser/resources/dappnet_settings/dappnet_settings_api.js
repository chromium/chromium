// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DappnetSettingsHandlerRemote} from './dappnet_settings.mojom-webui.js';

/**
 * JavaScript API for communicating with the Dappnet settings backend using Mojo.
 */
class DappnetSettingsApi {
  constructor() {
    this.handler = new DappnetSettingsHandlerRemote();
  }

  /**
   * Gets all configured RPC endpoints.
   * @return {Promise<Array>} Array of RPC endpoint objects
   */
  async getRpcEndpoints() {
    const {endpoints} = await this.handler.getRpcEndpoints();
    return endpoints;
  }

  /**
   * Adds a new RPC endpoint.
   * @param {Object} endpoint - The endpoint configuration
   * @param {string} endpoint.id - Unique identifier
   * @param {string} endpoint.name - Display name
   * @param {string} endpoint.url - RPC URL
   * @param {number} endpoint.chainId - Ethereum chain ID
   * @param {boolean} endpoint.isDefault - Whether this is the default endpoint
   * @return {Promise<{success: boolean, error?: string}>} Result
   */
  async addRpcEndpoint(endpoint) {
    const {success, error} = await this.handler.addRpcEndpoint(endpoint);
    return {success, error};
  }

  /**
   * Updates an existing RPC endpoint.
   * @param {string} id - The endpoint ID to update
   * @param {Object} endpoint - The updated endpoint configuration
   * @return {Promise<boolean>} Success status
   */
  async updateRpcEndpoint(id, endpoint) {
    const {success} = await this.handler.updateRpcEndpoint(id, endpoint);
    return success;
  }

  /**
   * Removes an RPC endpoint.
   * @param {string} id - The endpoint ID to remove
   * @return {Promise<boolean>} Success status
   */
  async removeRpcEndpoint(id) {
    const {success} = await this.handler.removeRpcEndpoint(id);
    return success;
  }

  /**
   * Tests connection to an RPC endpoint.
   * @param {string} url - The RPC URL to test
   * @return {Promise<{connected: boolean, error?: string}>} Connection result
   */
  async testRpcConnection(url) {
    const {connected, error} = await this.handler.testRpcConnection(url);
    return {connected, error};
  }

  /**
   * Sets the default RPC endpoint.
   * @param {string} id - The endpoint ID to set as default
   * @return {Promise<boolean>} Success status
   */
  async setDefaultRpc(id) {
    const {success} = await this.handler.setDefaultRpc(id);
    return success;
  }

  /**
   * Gets the current status of the local gateway.
   * @return {Promise<Object>} Gateway status object
   */
  async getGatewayStatus() {
    const {status} = await this.handler.getGatewayStatus();
    return status;
  }

  /**
   * Starts the local gateway process.
   * @return {Promise<{success: boolean, error?: string}>} Start result
   */
  async startGateway() {
    const {success, error} = await this.handler.startGateway();
    return {success, error};
  }

  /**
   * Stops the local gateway process.
   * @return {Promise<boolean>} Success status
   */
  async stopGateway() {
    const {success} = await this.handler.stopGateway();
    return success;
  }

  /**
   * Restarts the local gateway process.
   * @return {Promise<{success: boolean, error?: string}>} Restart result
   */
  async restartGateway() {
    const {success, error} = await this.handler.restartGateway();
    return {success, error};
  }

  /**
   * Gets the current status of the IPFS daemon.
   * @return {Promise<Object>} IPFS status object
   */
  async getIpfsStatus() {
    const {status} = await this.handler.getIpfsStatus();
    return status;
  }

  /**
   * Starts the IPFS daemon.
   * @return {Promise<{success: boolean, error?: string}>} Start result
   */
  async startIpfs() {
    const {success, error} = await this.handler.startIpfs();
    return {success, error};
  }

  /**
   * Stops the IPFS daemon.
   * @return {Promise<boolean>} Success status
   */
  async stopIpfs() {
    const {success} = await this.handler.stopIpfs();
    return success;
  }

  /**
   * Restarts the IPFS daemon.
   * @return {Promise<{success: boolean, error?: string}>} Restart result
   */
  async restartIpfs() {
    const {success, error} = await this.handler.restartIpfs();
    return {success, error};
  }
}

// Export the API class for use in other modules
window.DappnetSettingsApi = DappnetSettingsApi;