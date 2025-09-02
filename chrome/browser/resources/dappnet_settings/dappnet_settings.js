// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main JavaScript for the Dappnet Settings page.
 */

import './dappnet_settings_api.js';

class DappnetSettingsPage {
  constructor() {
    this.api = new DappnetSettingsApi();
    this.rpcEndpoints = [];
    this.statusUpdateInterval = null;
    
    // Initialize the page when DOM is ready
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => this.initialize());
    } else {
      this.initialize();
    }
  }

  /**
   * Initialize the settings page.
   */
  async initialize() {
    console.log('Initializing Dappnet Settings page');
    
    this.setupEventListeners();
    await this.loadRpcEndpoints();
    this.startStatusUpdates();
  }

  /**
   * Set up event listeners for UI interactions.
   */
  setupEventListeners() {
    // Add RPC endpoint button
    const addRpcBtn = document.getElementById('add-rpc-btn');
    addRpcBtn.addEventListener('click', () => this.showAddRpcModal());

    // Modal close buttons
    const closeModal = document.getElementById('close-modal');
    const cancelBtn = document.getElementById('cancel-btn');
    const modal = document.getElementById('add-rpc-modal');
    
    closeModal.addEventListener('click', () => this.hideAddRpcModal());
    cancelBtn.addEventListener('click', () => this.hideAddRpcModal());
    modal.addEventListener('click', (e) => {
      if (e.target === modal) this.hideAddRpcModal();
    });

    // Add RPC form
    const addRpcForm = document.getElementById('add-rpc-form');
    addRpcForm.addEventListener('submit', (e) => this.handleAddRpcSubmit(e));

    // Test connection button
    const testBtn = document.getElementById('test-connection-btn');
    testBtn.addEventListener('click', () => this.testRpcConnection());

    // Gateway control buttons
    document.getElementById('start-gateway-btn').addEventListener('click', 
      () => this.handleGatewayControl('start'));
    document.getElementById('stop-gateway-btn').addEventListener('click', 
      () => this.handleGatewayControl('stop'));
    document.getElementById('restart-gateway-btn').addEventListener('click', 
      () => this.handleGatewayControl('restart'));

    // IPFS control buttons
    document.getElementById('start-ipfs-btn').addEventListener('click', 
      () => this.handleIpfsControl('start'));
    document.getElementById('stop-ipfs-btn').addEventListener('click', 
      () => this.handleIpfsControl('stop'));
    document.getElementById('restart-ipfs-btn').addEventListener('click', 
      () => this.handleIpfsControl('restart'));
  }

  /**
   * Load and display RPC endpoints.
   */
  async loadRpcEndpoints() {
    try {
      this.rpcEndpoints = await this.api.getRpcEndpoints();
      this.renderRpcEndpoints();
    } catch (error) {
      console.error('Failed to load RPC endpoints:', error);
      this.showNotification('Failed to load RPC endpoints', 'error');
    }
  }

  /**
   * Render the RPC endpoints list.
   */
  renderRpcEndpoints() {
    const container = document.getElementById('rpc-endpoints-list');
    
    if (this.rpcEndpoints.length === 0) {
      container.innerHTML = `
        <div class="loading-placeholder">
          No RPC endpoints configured. Click "Add Endpoint" to get started.
        </div>
      `;
      return;
    }

    container.innerHTML = this.rpcEndpoints.map(endpoint => 
      this.renderRpcEndpoint(endpoint)
    ).join('');

    // Add event listeners to endpoint action buttons
    container.querySelectorAll('.rpc-endpoint').forEach(element => {
      const id = element.dataset.id;
      
      element.querySelector('.test-btn')?.addEventListener('click', 
        () => this.testEndpoint(id));
      element.querySelector('.edit-btn')?.addEventListener('click', 
        () => this.editEndpoint(id));
      element.querySelector('.delete-btn')?.addEventListener('click', 
        () => this.deleteEndpoint(id));
      element.querySelector('.default-btn')?.addEventListener('click', 
        () => this.setDefaultEndpoint(id));
    });
  }

  /**
   * Render a single RPC endpoint.
   */
  renderRpcEndpoint(endpoint) {
    return `
      <div class="rpc-endpoint ${endpoint.isDefault ? 'default' : ''}" data-id="${endpoint.id}">
        <div class="rpc-info">
          <span class="rpc-name">
            ${this.escapeHtml(endpoint.name)}
            ${endpoint.isDefault ? '<span class="default-badge">Default</span>' : ''}
          </span>
          <span class="rpc-url">${this.escapeHtml(endpoint.url)}</span>
          <span class="rpc-chain">Chain ID: ${endpoint.chainId}</span>
        </div>
        <div class="rpc-actions">
          <button class="secondary-button test-btn">Test</button>
          <button class="secondary-button edit-btn">Edit</button>
          <button class="secondary-button delete-btn">Delete</button>
          ${!endpoint.isDefault ? 
            '<button class="primary-button default-btn">Set Default</button>' : ''}
        </div>
      </div>
    `;
  }

  /**
   * Show the add RPC endpoint modal.
   */
  showAddRpcModal() {
    const modal = document.getElementById('add-rpc-modal');
    modal.classList.add('show');
    
    // Clear form
    document.getElementById('add-rpc-form').reset();
    
    // Focus first input
    document.getElementById('rpc-name').focus();
  }

  /**
   * Hide the add RPC endpoint modal.
   */
  hideAddRpcModal() {
    const modal = document.getElementById('add-rpc-modal');
    modal.classList.remove('show');
  }

  /**
   * Handle add RPC form submission.
   */
  async handleAddRpcSubmit(event) {
    event.preventDefault();
    
    const formData = new FormData(event.target);
    const endpoint = {
      id: this.generateId(),
      name: formData.get('name'),
      url: formData.get('url'),
      chainId: parseInt(formData.get('chainId')),
      isDefault: this.rpcEndpoints.length === 0 // First endpoint becomes default
    };

    try {
      const success = await this.api.addRpcEndpoint(endpoint);
      if (success) {
        this.showNotification('RPC endpoint added successfully', 'success');
        this.hideAddRpcModal();
        await this.loadRpcEndpoints();
      } else {
        this.showNotification('Failed to add RPC endpoint', 'error');
      }
    } catch (error) {
      console.error('Error adding RPC endpoint:', error);
      this.showNotification('Failed to add RPC endpoint', 'error');
    }
  }

  /**
   * Test RPC connection during endpoint creation.
   */
  async testRpcConnection() {
    const urlInput = document.getElementById('rpc-url');
    const testBtn = document.getElementById('test-connection-btn');
    
    if (!urlInput.value) {
      this.showNotification('Please enter a URL first', 'warning');
      return;
    }

    this.setButtonLoading(testBtn, true);
    
    try {
      const result = await this.api.testRpcConnection(urlInput.value);
      if (result.connected) {
        this.showNotification('Connection successful!', 'success');
      } else {
        this.showNotification(`Connection failed: ${result.error}`, 'error');
      }
    } catch (error) {
      console.error('Error testing connection:', error);
      this.showNotification('Connection test failed', 'error');
    } finally {
      this.setButtonLoading(testBtn, false);
    }
  }

  /**
   * Test an existing RPC endpoint.
   */
  async testEndpoint(id) {
    const endpoint = this.rpcEndpoints.find(ep => ep.id === id);
    if (!endpoint) return;

    const button = document.querySelector(`[data-id="${id}"] .test-btn`);
    this.setButtonLoading(button, true);

    try {
      const result = await this.api.testRpcConnection(endpoint.url);
      if (result.connected) {
        this.showNotification(`${endpoint.name} connection successful!`, 'success');
      } else {
        this.showNotification(`${endpoint.name} connection failed: ${result.error}`, 'error');
      }
    } catch (error) {
      console.error('Error testing endpoint:', error);
      this.showNotification(`Failed to test ${endpoint.name}`, 'error');
    } finally {
      this.setButtonLoading(button, false);
    }
  }

  /**
   * Delete an RPC endpoint.
   */
  async deleteEndpoint(id) {
    const endpoint = this.rpcEndpoints.find(ep => ep.id === id);
    if (!endpoint) return;

    if (!confirm(`Are you sure you want to delete "${endpoint.name}"?`)) {
      return;
    }

    try {
      const success = await this.api.removeRpcEndpoint(id);
      if (success) {
        this.showNotification('RPC endpoint deleted', 'success');
        await this.loadRpcEndpoints();
      } else {
        this.showNotification('Failed to delete RPC endpoint', 'error');
      }
    } catch (error) {
      console.error('Error deleting endpoint:', error);
      this.showNotification('Failed to delete RPC endpoint', 'error');
    }
  }

  /**
   * Set an endpoint as default.
   */
  async setDefaultEndpoint(id) {
    try {
      const success = await this.api.setDefaultRpc(id);
      if (success) {
        this.showNotification('Default RPC endpoint updated', 'success');
        await this.loadRpcEndpoints();
      } else {
        this.showNotification('Failed to update default RPC endpoint', 'error');
      }
    } catch (error) {
      console.error('Error setting default endpoint:', error);
      this.showNotification('Failed to update default RPC endpoint', 'error');
    }
  }

  /**
   * Handle gateway process control.
   */
  async handleGatewayControl(action) {
    const button = document.getElementById(`${action}-gateway-btn`);
    this.setButtonLoading(button, true);

    try {
      let result;
      switch (action) {
        case 'start':
          result = await this.api.startGateway();
          break;
        case 'stop':
          result = await this.api.stopGateway();
          break;
        case 'restart':
          result = await this.api.restartGateway();
          break;
      }

      if (result.success || result === true) {
        this.showNotification(`Gateway ${action} successful`, 'success');
      } else {
        this.showNotification(`Gateway ${action} failed: ${result.error}`, 'error');
      }
      
      await this.updateGatewayStatus();
    } catch (error) {
      console.error(`Error ${action}ing gateway:`, error);
      this.showNotification(`Gateway ${action} failed`, 'error');
    } finally {
      this.setButtonLoading(button, false);
    }
  }

  /**
   * Handle IPFS process control.
   */
  async handleIpfsControl(action) {
    const button = document.getElementById(`${action}-ipfs-btn`);
    this.setButtonLoading(button, true);

    try {
      let result;
      switch (action) {
        case 'start':
          result = await this.api.startIpfs();
          break;
        case 'stop':
          result = await this.api.stopIpfs();
          break;
        case 'restart':
          result = await this.api.restartIpfs();
          break;
      }

      if (result.success || result === true) {
        this.showNotification(`IPFS ${action} successful`, 'success');
      } else {
        this.showNotification(`IPFS ${action} failed: ${result.error}`, 'error');
      }
      
      await this.updateIpfsStatus();
    } catch (error) {
      console.error(`Error ${action}ing IPFS:`, error);
      this.showNotification(`IPFS ${action} failed`, 'error');
    } finally {
      this.setButtonLoading(button, false);
    }
  }

  /**
   * Start periodic status updates.
   */
  startStatusUpdates() {
    // Update status immediately
    this.updateGatewayStatus();
    this.updateIpfsStatus();
    
    // Then update every 5 seconds
    this.statusUpdateInterval = setInterval(() => {
      this.updateGatewayStatus();
      this.updateIpfsStatus();
    }, 5000);
  }

  /**
   * Update gateway status display.
   */
  async updateGatewayStatus() {
    try {
      const status = await this.api.getGatewayStatus();
      
      const statusElement = document.getElementById('gateway-status');
      const dot = statusElement.querySelector('.status-dot');
      const text = statusElement.querySelector('.status-text');
      
      dot.className = `status-dot ${status.isRunning ? 'running' : 'stopped'}`;
      text.textContent = status.isRunning ? 'Running' : 'Stopped';
      
      document.getElementById('gateway-port').textContent = status.port || '-';
      document.getElementById('gateway-pid').textContent = status.pid || '-';
    } catch (error) {
      console.error('Failed to update gateway status:', error);
    }
  }

  /**
   * Update IPFS status display.
   */
  async updateIpfsStatus() {
    try {
      const status = await this.api.getIpfsStatus();
      
      const statusElement = document.getElementById('ipfs-status');
      const dot = statusElement.querySelector('.status-dot');
      const text = statusElement.querySelector('.status-text');
      
      dot.className = `status-dot ${status.isRunning ? 'running' : 'stopped'}`;
      text.textContent = status.isRunning ? 'Running' : 'Stopped';
      
      document.getElementById('ipfs-api-port').textContent = status.apiPort || '-';
      document.getElementById('ipfs-gateway-port').textContent = status.gatewayPort || '-';
      document.getElementById('ipfs-peer-count').textContent = status.peerCount || '-';
    } catch (error) {
      console.error('Failed to update IPFS status:', error);
    }
  }

  /**
   * Show a notification to the user.
   */
  showNotification(message, type = 'info') {
    const container = document.getElementById('notifications');
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    
    container.appendChild(notification);
    
    // Auto-remove after 5 seconds
    setTimeout(() => {
      notification.remove();
    }, 5000);
  }

  /**
   * Set loading state on a button.
   */
  setButtonLoading(button, loading) {
    if (loading) {
      button.disabled = true;
      button.dataset.originalText = button.textContent;
      button.textContent = 'Loading...';
      button.classList.add('loading');
    } else {
      button.disabled = false;
      button.textContent = button.dataset.originalText;
      button.classList.remove('loading');
    }
  }

  /**
   * Generate a unique ID.
   */
  generateId() {
    return 'rpc_' + Math.random().toString(36).substring(2, 15);
  }

  /**
   * Escape HTML to prevent XSS.
   */
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  /**
   * Cleanup when page is unloaded.
   */
  destroy() {
    if (this.statusUpdateInterval) {
      clearInterval(this.statusUpdateInterval);
    }
  }
}

// Initialize the page when loaded
const dappnetSettings = new DappnetSettingsPage();

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
  dappnetSettings.destroy();
});