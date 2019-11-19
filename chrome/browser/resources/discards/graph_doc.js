// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Target y position for page nodes.
const kPageNodesTargetY = 20;

// Range occupied by page nodes at the top of the graph view.
const kPageNodesYRange = 100;

// Range occupied by process nodes at the bottom of the graph view.
const kProcessNodesYRange = 150;

// Target y position for frame nodes.
const kFrameNodesTargetY = kPageNodesYRange + 50;

// Range that frame nodes cannot enter at the top/bottom of the graph view.
const kFrameNodesTopMargin = kPageNodesYRange;
const kFrameNodesBottomMargin = kProcessNodesYRange + 50;

/** @implements {d3.ForceNode} */
class GraphNode {
  constructor(id) {
    /** @type {number} */
    this.id = id;
    /** @type {string} */
    this.color = 'black';
    /** @type {string} */
    this.iconUrl = '';

    /**
     * Implementation of the d3.ForceNode interface.
     * See https://github.com/d3/d3-force#simulation_nodes.
     * @type {number|undefined}
     */
    this.index;
    /** @type {number|undefined} */
    this.x;
    /** @type {number|undefined} */
    this.y;
    /** @type {number|undefined} */
    this.vx;
    /** @type {number|undefined} */
    this.vy;
    this.fx = null;
    this.fy = null;
  }

  /** @return {string} */
  get title() {
    return '';
  }

  /**
   * Sets the initial x and y position of this node, also resets
   * vx and vy.
   * @param {number} graph_width: Width of the graph view (svg).
   * @param {number} graph_height: Height of the graph view (svg).
   */
  setInitialPosition(graph_width, graph_height) {
    this.x = graph_width / 2;
    this.y = this.targetYPosition(graph_height);
    this.vx = 0;
    this.vy = 0;
  }

  /**
   * @param {number} graph_height: Height of the graph view (svg).
   * @return {number}
   */
  targetYPosition(graph_height) {
    const bounds = this.allowedYRange(graph_height);
    return (bounds[0] + bounds[1]) / 2;
  }

  /**
   * @return {number}: The strength of the force that pulls the node towards
   *                    its target y position.
   */
  targetYPositionStrength() {
    return 0.1;
  }

  /**
   * @param {number} graph_height: Height of the graph view.
   * @return {!Array<number>}
   */
  allowedYRange(graph_height) {
    // By default, nodes just need to be in bounds of the graph.
    return [0, graph_height];
  }

  /** @return {number}: The strength of the repulsion force with other nodes. */
  manyBodyStrength() {
    return -200;
  }

  /** @return {!Array<number>} */
  linkTargets() {
    return [];
  }

  /**
   * Selects a color string from an id.
   * @param {number} id: The id the returned color is selected from.
   * @return {string}
   */
  selectColor(id) {
    return d3.schemeSet3[Math.abs(id) % 12];
  }
}

class PageNode extends GraphNode {
  /** @param {!discards.mojom.PageInfo} page */
  constructor(page) {
    super(page.id);
    /** @type {!discards.mojom.PageInfo} */
    this.page = page;
    this.y = kPageNodesTargetY;
  }

  /** override */
  get title() {
    return this.page.mainFrameUrl.url.length > 0 ? this.page.mainFrameUrl.url :
                                                   'Page';
  }

  /** @override */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graph_height) {
    return [0, kPageNodesYRange];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }
}

class FrameNode extends GraphNode {
  /** @param {!discards.mojom.FrameInfo} frame */
  constructor(frame) {
    super(frame.id);
    /** @type {!discards.mojom.FrameInfo} frame */
    this.frame = frame;
    this.color = this.selectColor(frame.processId);
  }

  /** override */
  get title() {
    return this.frame.url.url.length > 0 ? this.frame.url.url : 'Frame';
  }

  /** override */
  targetYPosition(graph_height) {
    return kFrameNodesTargetY;
  }

  /** override */
  allowedYRange(graph_height) {
    return [kFrameNodesTopMargin, graph_height - kFrameNodesBottomMargin];
  }

  /** override */
  linkTargets() {
    // Only link to the page if there isn't a parent frame.
    return [
      this.frame.parentFrameId || this.frame.pageId, this.frame.processId
    ];
  }
}

class ProcessNode extends GraphNode {
  /** @param {!discards.mojom.ProcessInfo} process */
  constructor(process) {
    super(process.id);
    /** @type {!discards.mojom.ProcessInfo} */
    this.process = process;

    this.color = this.selectColor(process.id);
  }

  /** override */
  get title() {
    return `PID: ${this.process.pid.pid}`;
  }

  /** @return {number} */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graph_height) {
    return [graph_height - kProcessNodesYRange, graph_height];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }
}

/**
 * A force that bounds GraphNodes |allowedYRange| in Y.
 * @param {number} graph_height
 */
function bounding_force(graph_height) {
  /** @type {!Array<!GraphNode>} */
  let nodes = [];
  /** @type {!Array<!Array>} */
  let bounds = [];

  /** @param {number} alpha */
  function force(alpha) {
    const n = nodes.length;
    for (let i = 0; i < n; ++i) {
      const bound = bounds[i];
      const node = nodes[i];
      const yOld = node.y;
      const yNew = Math.max(bound[0], Math.min(yOld, bound[1]));
      if (yOld != yNew) {
        node.y = yNew;
        // Zero the velocity of clamped nodes.
        node.vy = 0;
      }
    }
  }

  /** @param {!Array<!GraphNode>} n */
  force.initialize = function(n) {
    nodes = n;
    bounds = nodes.map(node => node.allowedYRange(graph_height));
  };

  return force;
}

/**
 * @implements {discards.mojom.GraphChangeStreamInterface}
 */
class Graph {
  /**
   * TODO(siggi): This should be SVGElement, but closure doesn't have externs
   *    for this yet.
   * @param {Element} svg
   */
  constructor(svg) {
    /**
     * TODO(siggi): SVGElement.
     * @private {Element}
     */
    this.svg_ = svg;

    /** @private {boolean} */
    this.wasResized_ = false;

    /** @private {number} */
    this.width_ = 0;
    /** @private {number} */
    this.height_ = 0;

    /** @private {d3.ForceSimulation} */
    this.simulation_ = null;

    /**
     * A selection for the top-level <g> node that contains all nodes.
     * @private {d3.selection}
     */
    this.nodeGroup_ = null;

    /**
     * A selection for the top-level <g> node that contains all edges.
     * @private {d3.selection}
     */
    this.linkGroup_ = null;

    /** @private {!Map<number, !GraphNode>} */
    this.nodes_ = new Map();

    /**
     * The links.
     * @private {!Array<!d3.ForceLink>}
     */
    this.links_ = [];
  }

  initialize() {
    // Set up a message listener to receive the graph data from the WebUI.
    // This is hosted in a webview that is never navigated anywhere else,
    // so these event handlers are never removed.
    window.addEventListener('message', this.onMessage_.bind(this));

    // Set up a window resize listener to track the graph on resize.
    window.addEventListener('resize', this.onResize_.bind(this));

    // Create the simulation and set up the permanent forces.
    const simulation = d3.forceSimulation();
    simulation.on('tick', this.onTick_.bind(this));

    const linkForce = d3.forceLink().id(d => d.id);
    simulation.force('link', linkForce);

    // Sets the repulsion force between nodes (positive number is attraction,
    // negative number is repulsion).
    simulation.force(
        'charge',
        d3.forceManyBody().strength(this.getManyBodyStrength_.bind(this)));

    this.simulation_ = simulation;

    // Create the <g> elements that host nodes and links.
    // The link group is created first so that all links end up behind nodes.
    const svg = d3.select(this.svg_);
    this.linkGroup_ = svg.append('g').attr('class', 'links');
    this.nodeGroup_ = svg.append('g').attr('class', 'nodes');
  }

  /** @override */
  frameCreated(frame) {
    this.addNode_(new FrameNode(frame));
  }

  /** @override */
  pageCreated(page) {
    this.addNode_(new PageNode(page));
  }

  /** @override */
  processCreated(process) {
    this.addNode_(new ProcessNode(process));
  }

  /** @override */
  frameChanged(frame) {
    const frameNode = /** @type {!FrameNode} */ (this.nodes_.get(frame.id));
    frameNode.frame = frame;
  }

  /** @override */
  pageChanged(page) {
    const pageNode = /** @type {!PageNode} */ (this.nodes_.get(page.id));
    pageNode.page = page;
  }

  /** @override */
  processChanged(process) {
    const processNode =
        /** @type {!ProcessNode} */ (this.nodes_.get(process.id));
    processNode.process = process;
  }

  /** @override */
  favIconDataAvailable(iconInfo) {
    const graphNode = this.nodes_.get(iconInfo.nodeId);
    if (graphNode) {
      graphNode.iconUrl = 'data:image/png;base64,' + iconInfo.iconData;
    }
  }

  /** @override */
  nodeDeleted(nodeId) {
    const node = this.nodes_.get(nodeId);

    // Filter away any links to or from the deleted node.
    this.links_ =
        this.links_.filter(link => link.source != node && link.target != node);

    // And remove the node.
    this.nodes_.delete(nodeId);
  }

  /**
   * @param {!Event} event A graph update event posted from the WebUI.
   * @private
   */
  onMessage_(event) {
    const type = /** @type {string} */ (event.data[0]);
    const data = /** @type {Object|number} */ (event.data[1]);
    switch (type) {
      case 'frameCreated':
        this.frameCreated(
            /** @type {!discards.mojom.FrameInfo} */ (data));
        break;
      case 'pageCreated':
        this.pageCreated(
            /** @type {!discards.mojom.PageInfo} */ (data));
        break;
      case 'processCreated':
        this.processCreated(
            /** @type {!discards.mojom.ProcessInfo} */ (data));
        break;
      case 'frameChanged':
        this.frameChanged(
            /** @type {!discards.mojom.FrameInfo} */ (data));
        break;
      case 'pageChanged':
        this.pageChanged(
            /** @type {!discards.mojom.PageInfo} */ (data));
        break;
      case 'processChanged':
        this.processChanged(
            /** @type {!discards.mojom.ProcessInfo} */ (data));
        break;
      case 'favIconDataAvailable':
        this.favIconDataAvailable(
            /** @type {!discards.mojom.FavIconInfo} */ (data));
        break;
      case 'nodeDeleted':
        this.nodeDeleted(/** @type {number} */ (data));
        break;
    }

    this.render_();
  }

  /**
   * Renders nodes_ and edges_ to the SVG DOM.
   *
   * Each edge is a line element.
   * Each node is represented as a group element with three children:
   *   1. A circle that has a color and which animates the node on creation
   *      and deletion.
   *   2. An image that is provided a data URL for the nodes favicon, when
   *      available.
   *   3. A title element that presents the nodes URL on hover-over, if
   *      available.
   * Deleted nodes are classed '.dead', and CSS takes care of hiding their
   * image element if it's been populated with an icon.
   *
   * @private
   */
  render_() {
    // Select the links.
    const link = this.linkGroup_.selectAll('line').data(this.links_);
    // Add new links.
    link.enter().append('line').attr('stroke-width', 1);
    // Remove dead links.
    link.exit().remove();

    // Select the nodes, except for any dead ones that are still transitioning.
    const nodes = Array.from(this.nodes_.values());
    const node =
        this.nodeGroup_.selectAll('g:not(.dead)').data(nodes, d => d.id);

    // Add new nodes, if any.
    if (!node.enter().empty()) {
      const drag = d3.drag();
      drag.on('start', this.onDragStart_.bind(this));
      drag.on('drag', this.onDrag_.bind(this));
      drag.on('end', this.onDragEnd_.bind(this));

      const newNodes = node.enter().append('g').call(drag);
      const circles = newNodes.append('circle').attr('r', 9).attr(
          'fill', 'green');  // New nodes appear green.
      newNodes.append('image')
          .attr('x', -8)
          .attr('y', -8)
          .attr('width', 16)
          .attr('height', 16);
      newNodes.append('title');

      // Transition new nodes to their chosen color in 2 seconds.
      circles.transition()
          .duration(2000)
          .attr('fill', d => d.color)
          .attr('r', 6);
    }

    // Give dead nodes a distinguishing class to exclude them from the selection
    // above. Interrupt any ongoing transitions, then transition them out.
    const deletedNodes = node.exit().classed('dead', true).interrupt();

    deletedNodes.select('circle')
        .attr('r', 9)
        .attr('fill', 'red')
        .transition()
        .duration(2000)
        .attr('r', 0)
        .remove();

    // Update the title for all nodes.
    node.selectAll('title').text(d => d.title);
    // Update the favicon for all nodes.
    node.selectAll('image').attr('href', d => d.iconUrl);

    // Update and restart the simulation if the graph changed.
    if (!node.enter().empty() || !node.exit().empty() ||
        !link.enter().empty() || !link.exit().empty()) {
      this.simulation_.nodes(nodes);
      this.simulation_.force('link').links(this.links_);

      this.restartSimulation_();
    }
  }

  /** @private */
  onTick_() {
    const nodes = this.nodeGroup_.selectAll('g');
    nodes.attr('transform', d => `translate(${d.x},${d.y})`);

    const lines = this.linkGroup_.selectAll('line');
    lines.attr('x1', d => d.source.x)
        .attr('y1', d => d.source.y)
        .attr('x2', d => d.target.x)
        .attr('y2', d => d.target.y);
  }

  /**
   * @param {!GraphNode} source
   * @param {number} dst_id
   * @private
   */
  maybeAddLink_(source, dst_id) {
    const target = this.nodes_.get(dst_id);
    if (target) {
      this.links_.push({source: source, target: target});
    }
  }

  /**
   * Adds a new node to the graph, populates its links and gives it an initial
   * position.
   *
   * @param {!GraphNode} node
   * @private
   */
  addNode_(node) {
    this.nodes_.set(node.id, node);

    const linkTargets = node.linkTargets();
    for (const linkTarget of linkTargets) {
      this.maybeAddLink_(node, linkTarget);
    }

    node.setInitialPosition(this.width_, this.height_);
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDragStart_(d) {
    if (!d3.event.active) {
      this.restartSimulation_();
    }
    d.fx = d.x;
    d.fy = d.y;
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDrag_(d) {
    d.fx = d3.event.x;
    d.fy = d3.event.y;
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDragEnd_(d) {
    if (!d3.event.active) {
      this.simulation_.alphaTarget(0);
    }
    d.fx = null;
    d.fy = null;
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getTargetYPosition_(d) {
    return d.targetYPosition(this.height_);
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getTargetYPositionStrength_(d) {
    return d.targetYPositionStrength();
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getManyBodyStrength_(d) {
    return d.manyBodyStrength();
  }

  /** @private */
  restartSimulation_() {
    // Restart the simulation.
    this.simulation_.alphaTarget(0.3).restart();
  }

  /**
   * Resizes and restarts the animation after a size change.
   * @private
   */
  onResize_() {
    this.width_ = this.svg_.clientWidth;
    this.height_ = this.svg_.clientHeight;

    // Reset both X and Y attractive forces, as they're cached.
    const xForce = d3.forceX().x(this.width_ / 2).strength(0.1);
    const yForce = d3.forceY()
                       .y(this.getTargetYPosition_.bind(this))
                       .strength(this.getTargetYPositionStrength_.bind(this));
    this.simulation_.force('x_pos', xForce);
    this.simulation_.force('y_pos', yForce);
    this.simulation_.force('y_bound', bounding_force(this.height_));

    if (!this.wasResized_) {
      this.wasResized_ = true;

      // Reinitialize all node positions on first resize.
      this.nodes_.forEach(
          node => node.setInitialPosition(this.width_, this.height_));

      // Allow the simulation to settle by running it for a bit.
      for (let i = 0; i < 200; ++i) {
        this.simulation_.tick();
      }
    }

    this.restartSimulation_();
  }
}

let graph = null;
function onLoad() {
  graph = new Graph(document.querySelector('svg'));

  graph.initialize();
}

window.addEventListener('load', onLoad);
