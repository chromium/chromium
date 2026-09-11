// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/d3/d3.min.js';

import {assert} from 'chrome://resources/js/assert.js';

import type {FavIconInfo, FrameInfo, GraphChangeStreamInterface, PageInfo, ProcessInfo, WorkerInfo} from './discards.mojom-webui.js';

// Radius of a node circle.
const kNodeRadius: number = 6;

// Target y position for page nodes.
const kPageNodesTargetY: number = 20;

// Range occupied by page nodes at the top of the graph view.
const kPageNodesYRange: number = 100;

// Range occupied by process nodes at the bottom of the graph view.
const kProcessNodesYRange: number = 100;

// Range occupied by worker nodes at the bottom of the graph view, above
// process nodes.
const kWorkerNodesYRange: number = 200;

// Target y position for frame nodes.
const kFrameNodesTargetY: number = kPageNodesYRange + 50;

// Range that frame nodes cannot enter at the top/bottom of the graph view.
const kFrameNodesTopMargin: number = kPageNodesYRange;
const kFrameNodesBottomMargin: number = kWorkerNodesYRange + 50;

// The maximum strength of a boundary force.
// According to https://github.com/d3/d3-force#positioning, strength values
// outside the range [0,1] are "not recommended".
const kMaxBoundaryStrength: number = 1;

// The strength of a high Y-force. This is appropriate for forces that
// strongly pull towards an attractor, but can still be overridden by the
// strongest force.
const kHighYStrength: number = 0.9;

// The strength of a weak Y-force. This is appropriate for forces that exert
// some influence but can be easily overridden.
const kWeakYStrength: number = 0.1;


interface ToolTipHeadingRowData {
  rowClass: 'heading';
  describerName: string;
  itemCount: number;
}

interface ToolTipValueRowData {
  rowClass: 'value';
  describerName: string;
  key: string;
  value: string;
  fullValue?: string;
}

interface ToolTipLoadingRowData {
  rowClass: 'loading';
}

interface ToolTipEmptyRowData {
  rowClass: 'empty';
}

type ToolTipRowData = ToolTipHeadingRowData|ToolTipValueRowData|
    ToolTipLoadingRowData|ToolTipEmptyRowData;

class ToolTip {
  floating: boolean = true;
  x: number;
  y: number;
  node: GraphNode;
  private graph_: Graph;
  private div_: d3.Selection<HTMLDivElement, unknown, null, undefined>;
  private titleSpan_: d3.Selection<HTMLSpanElement, unknown, null, undefined>;
  private descriptionJson_: string|null = null;
  private collapsedSections_: Map<string, boolean> = new Map();

  constructor(div: Element, node: GraphNode, graph: Graph) {
    this.x = node.x;
    this.y = node.y - 28;
    this.node = node;

    this.graph_ = graph;
    this.div_ = d3.select(div)
                    .append('div')
                    .attr('class', 'tooltip')
                    .style('opacity', 0)
                    .style('left', `${this.x}px`)
                    .style('top', `${this.y}px`);

    const header = this.div_.append('div').attr('class', 'tooltip-header');
    const title = this.node.title || 'Node';
    this.titleSpan_ = header.append('span')
                          .attr('class', 'tooltip-title')
                          .attr('title', title)
                          .text(title);

    header.append('button')
        .attr('class', 'tooltip-close-button')
        .attr('title', 'Close')
        .attr('aria-label', 'Close')
        .text('✕')
        .on('click', (event: MouseEvent) => {
          event.stopPropagation();
          this.graph_.closeToolTip(this.node);
        });

    const content = this.div_.append('div').attr('class', 'tooltip-content');
    const tbody = content.append('table').append('tbody');
    tbody.append('tr')
        .datum<ToolTipRowData>({rowClass: 'loading'})
        .attr('class', 'loading')
        .append('td')
        .attr('colspan', 2)
        .text('Loading...');
    this.div_.transition().duration(200).style('opacity', .9);

    // Set up a drag behavior for this object's div, filtering to the header.
    const drag = d3.drag().subject(() => this) as unknown as
        d3.DragBehavior<HTMLDivElement, unknown, unknown>;
    drag.filter((event: MouseEvent) => {
      const target = event.target as HTMLElement | null;
      return !event.ctrlKey && !event.button &&
          !!target?.closest('.tooltip-header') &&
          !target?.closest('.tooltip-close-button');
    });
    drag.on('start', this.onDragStart_.bind(this));
    drag.on('drag', this.onDrag_.bind(this));
    this.div_.call(drag);
  }

  nodeMoved() {
    if (!this.floating) {
      return;
    }

    const node = this.node;
    this.x = node.x;
    this.y = node.y - 28;
    this.div_.style('left', `${this.x}px`).style('top', `${this.y}px`);
  }

  /**
   * @return The [x, y] center of the ToolTip's div element.
   */
  getCenter(): [number, number] {
    const rect = this.div_.node()!.getBoundingClientRect();
    return [rect.x + rect.width / 2, rect.y + rect.height / 2];
  }

  goAway() {
    this.div_.transition().duration(200).style('opacity', 0).remove();
  }

  private isSectionCollapsed_(describerName: string): boolean {
    if (this.collapsedSections_.has(describerName)) {
      return this.collapsedSections_.get(describerName)!;
    }
    // Default describer starts expanded, all decorator describers start
    // collapsed.
    return describerName !== this.node.defaultDescriberName;
  }

  private toggleSection_(describerName: string) {
    const nextState = !this.isSectionCollapsed_(describerName);
    this.collapsedSections_.set(describerName, nextState);
    this.updateRowVisibility_();
    this.graph_.updateToolTipLinks();
  }

  private updateRowVisibility_() {
    this.div_.selectAll<HTMLTableRowElement, ToolTipRowData>('tbody tr')
        .each((d, i, nodes) => {
          const el = nodes[i];
          if (!d || !el) {
            return;
          }
          if (d.rowClass === 'heading') {
            const isEmpty = d.itemCount === 0;
            const isCollapsed = this.isSectionCollapsed_(d.describerName);
            const icon = isEmpty ? '' : `${isCollapsed ? '▸' : '▾'} `;
            const countStr = isEmpty ? ' (empty)' : ` (${d.itemCount})`;
            const text = `${icon}${d.describerName}${countStr}`;
            const td = el.querySelector('td');
            if (td) {
              let button = td.querySelector<HTMLButtonElement>('button');
              if (isEmpty) {
                if (button) {
                  button.remove();
                }
                if (td.textContent !== text) {
                  td.textContent = text;
                }
              } else {
                if (!button) {
                  td.textContent = '';
                  button = document.createElement('button');
                  button.className = 'section-toggle';
                  button.addEventListener('click', () => {
                    this.toggleSection_(d.describerName);
                  });
                  td.appendChild(button);
                }
                if (button.textContent !== text) {
                  button.textContent = text;
                }
                button.setAttribute('aria-expanded', String(!isCollapsed));
              }
            }
            el.classList.toggle('empty', isEmpty);
          } else if (d.rowClass === 'value') {
            el.classList.toggle(
                'collapsed', this.isSectionCollapsed_(d.describerName));
          }
        });
  }

  private flattenObjectRec_(
      visited: Set<object>, flattened: ToolTipRowData[], describerName: string,
      prefix: string, object: object) {
    if (!object || typeof object !== 'object' || visited.has(object)) {
      return;
    }
    visited.add(object);

    const sortedEntries =
        Object.entries(object).sort(([a], [b]) => a.localeCompare(b));

    for (const [key, value] of sortedEntries) {
      const fullKey = prefix ? `${prefix}.${key}` : key;
      if (value && typeof value === 'object' && !Array.isArray(value)) {
        this.flattenObjectRec_(
            visited, flattened, describerName, fullKey, value);
      } else {
        const rawString = Array.isArray(value) ?
            (value.length === 0 ? '[]' : value.join(', ')) :
            String(value);

        let displayValue = rawString;
        let fullValue: string|undefined;
        if (rawString.length > 50) {
          displayValue = `${rawString.substring(0, 47)}...`;
          fullValue = rawString;
        }

        flattened.push({
          rowClass: 'value',
          describerName,
          key: fullKey,
          value: displayValue,
          ...(fullValue ? {fullValue} : {}),
        });
      }
    }
  }

  private flattenDescription_(object: Record<string, object>):
      ToolTipRowData[] {
    const flattened: ToolTipRowData[] = [];
    const visited = new Set<object>();
    const defaultDescriber = this.node.defaultDescriberName;

    // Sort top-level entries so the default describer is first, followed by
    // decorators alphabetically.
    const describerNames = Object.keys(object).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (a === defaultDescriber) {
        return -1;
      }
      if (b === defaultDescriber) {
        return 1;
      }
      return a.localeCompare(b);
    });

    for (const describerName of describerNames) {
      const value = object[describerName];
      if (value && typeof value === 'object') {
        const headingIndex = flattened.length;
        const heading: ToolTipHeadingRowData = {
          rowClass: 'heading',
          describerName,
          itemCount: 0,
        };
        flattened.push(heading);

        this.flattenObjectRec_(visited, flattened, describerName, '', value);

        heading.itemCount = flattened.length - headingIndex - 1;
      }
    }
    return flattened;
  }

  /**
   * Updates the description displayed.
   */
  onDescription(descriptionJson: string) {
    const title = this.node.title || 'Node';
    if (this.titleSpan_.text() !== title) {
      this.titleSpan_.text(title);
      this.titleSpan_.attr('title', title);
    }

    if (this.descriptionJson_ === descriptionJson) {
      return;
    }

    this.descriptionJson_ = descriptionJson;
    let data: Record<string, object> = {};
    try {
      const parsed = JSON.parse(descriptionJson);
      if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
        data = parsed;
      }
    } catch {
      // Ignore parse errors.
    }

    const flattenedDescription = this.flattenDescription_(data);
    if (flattenedDescription.length === 0) {
      flattenedDescription.push({rowClass: 'empty'});
    }

    const tbody = this.div_.select<HTMLTableSectionElement>('tbody');
    const tbodyNode = tbody.node()!;
    const existingRows =
        Array.from(tbodyNode.children) as HTMLTableRowElement[];

    const canUpdateInPlace =
        existingRows.length === flattenedDescription.length &&
        flattenedDescription.every((rowData, i) => {
          const tr = existingRows[i]!;
          const currentData =
              d3.select(tr).datum() as ToolTipRowData | undefined;
          if (!currentData || currentData.rowClass !== rowData.rowClass) {
            return false;
          }
          if (rowData.rowClass === 'heading' &&
              currentData.rowClass === 'heading') {
            return currentData.describerName === rowData.describerName;
          }
          if (rowData.rowClass === 'value' &&
              currentData.rowClass === 'value') {
            return currentData.key === rowData.key;
          }
          return true;
        });

    if (canUpdateInPlace) {
      for (let i = 0; i < flattenedDescription.length; ++i) {
        const rowData = flattenedDescription[i]!;
        const tr = existingRows[i]!;
        d3.select(tr).datum(rowData);
        if (rowData.rowClass === 'value') {
          const valTd = tr.children[1] as HTMLElement | undefined;
          if (valTd) {
            if (valTd.textContent !== rowData.value) {
              valTd.textContent = rowData.value;
            }
            const expectedTitle = rowData.fullValue || '';
            if (valTd.title !== expectedTitle) {
              valTd.title = expectedTitle;
            }
          }
        }
      }
    } else {
      tbody.selectAll('*').remove();
      for (const rowData of flattenedDescription) {
        const tr =
            tbody.append('tr').datum(rowData).attr('class', rowData.rowClass);

        if (rowData.rowClass === 'heading') {
          tr.append('td').attr('colspan', 2);
        } else if (rowData.rowClass === 'loading') {
          tr.append('td').attr('colspan', 2).text('Loading...');
        } else if (rowData.rowClass === 'empty') {
          tr.append('td').attr('colspan', 2).text('No data');
        } else {
          tr.append('td').text(rowData.key);
          const valTd = tr.append('td').text(rowData.value);
          if (rowData.fullValue) {
            valTd.attr('title', rowData.fullValue);
          }
        }
      }
    }

    this.updateRowVisibility_();
    this.graph_.updateToolTipLinks();
  }

  private onDragStart_() {
    this.floating = false;
  }

  private onDrag_(event: {x: number, y: number}) {
    this.x = event.x;
    this.y = event.y;
    this.div_.style('left', `${this.x}px`).style('top', `${this.y}px`);

    this.graph_.updateToolTipLinks();
  }
}

class GraphNode implements d3.SimulationNodeDatum {
  id: bigint;
  color: string = 'black';
  iconUrl: string = '';
  tooltip: ToolTip|null = null;

  /**
   * Implementation of the d3.SimulationNodeDatum interface.
   * See https://github.com/d3/d3-force#simulation_nodes.
   */
  index?: number;
  x: number = 0;
  y: number = 0;
  vx?: number;
  vy?: number;
  fx: number|null = null;
  fy: number|null = null;


  constructor(id: bigint) {
    this.id = id;
  }

  get title(): string {
    return '';
  }

  get defaultDescriberName(): string {
    return '';
  }

  /**
   * Sets the initial x and y position of this node, also resets
   * vx and vy.
   * @param graphWidth Width of the graph view (svg).
   * @param graphHeight Height of the graph view (svg).
   */
  setInitialPosition(graphWidth: number, graphHeight: number) {
    this.x = graphWidth / 2;
    this.y = this.targetPositionY(graphHeight);
    this.vx = 0;
    this.vy = 0;
  }

  /**
   * @param graphHeight Height of the graph view (svg).
   */
  targetPositionY(graphHeight: number): number {
    const bounds = this.allowedRangeY(graphHeight);
    return (bounds[0] + bounds[1]) / 2;
  }

  /**
   * @return The strength of the force that pulls the node towards
   *     its target y position.
   */
  get targetYPositionStrength(): number {
    return kWeakYStrength;
  }

  /**
   * @return A scaling factor applied to the strength of links to this
   *     node.
   */
  get linkStrengthScalingFactor(): number {
    return 1;
  }

  /**
   * @param graphHeight Height of the graph view.
   */
  allowedRangeY(graphHeight: number): [number, number] {
    // By default, nodes just need to be in bounds of the graph.
    return [0, graphHeight];
  }

  /** @return The strength of the repulsion force with other nodes. */
  get manyBodyStrength(): number {
    return -200;
  }

  /** @return an array of node ids. */
  get linkTargets(): bigint[] {
    return [];
  }

  /**
   * Dashed links express ownership relationships. An object can own multiple
   * things, but be owned by exactly one (per relationship type). As such, the
   * relationship is expressed on the *owned* object. These links are drawn with
   * an arrow at the beginning of the link, pointing to the owned object.
   * @return an array of node ids.
   */
  get dashedLinkTargets(): bigint[] {
    return [];
  }

  /**
   * Selects a color string from an id.
   * @param id The id the returned color is selected from.
   */
  selectColor(id: bigint): string {
    if (id < 0) {
      id = -id;
    }
    const color = d3.schemeSet3[Number(id % BigInt(12))];
    assert(color);
    return color;
  }
}

class PageNode extends GraphNode {
  page: PageInfo;

  constructor(page: PageInfo) {
    super(page.id);
    this.page = page;
    this.y = kPageNodesTargetY;
  }

  override get title() {
    return this.page.mainFrameUrl.length > 0 ? this.page.mainFrameUrl : 'Page';
  }

  override get defaultDescriberName() {
    return 'PageNodeImpl';
  }

  override get targetYPositionStrength() {
    // Gravitate strongly towards the top of the graph. Can be overridden by
    // the bounding force which uses kMaxBoundaryStrength.
    return kHighYStrength;
  }

  override get linkStrengthScalingFactor() {
    // Give links from frame nodes to page nodes less weight than links between
    // frame nodes, so the that Y forces pulling page nodes into their area can
    // dominate over link forces pulling them towards frame nodes.
    return 0.5;
  }

  override allowedRangeY(_graphHeight: number): [number, number] {
    return [0, kPageNodesYRange];
  }

  override get manyBodyStrength() {
    return -600;
  }

  override get dashedLinkTargets() {
    const targets = [];
    if (this.page.openerFrameId) {
      targets.push(this.page.openerFrameId);
    }
    if (this.page.embedderFrameId) {
      targets.push(this.page.embedderFrameId);
    }
    return targets;
  }
}

class FrameNode extends GraphNode {
  frame: FrameInfo;

  constructor(frame: FrameInfo) {
    super(frame.id);
    this.frame = frame;
    this.color = this.selectColor(frame.processId);
  }

  override get title() {
    return this.frame.url.length > 0 ? this.frame.url : 'Frame';
  }

  override get defaultDescriberName() {
    return 'FrameNodeImpl';
  }

  override targetPositionY(_graphHeight: number) {
    return kFrameNodesTargetY;
  }

  override allowedRangeY(graphHeight: number): [number, number] {
    return [kFrameNodesTopMargin, graphHeight - kFrameNodesBottomMargin];
  }

  override get linkTargets() {
    // Only link to the page if there isn't a parent frame.
    return [
      this.frame.parentFrameId || this.frame.pageId,
      this.frame.processId,
    ];
  }
}

class ProcessNode extends GraphNode {
  process: ProcessInfo;

  constructor(process: ProcessInfo) {
    super(process.id);
    this.process = process;

    this.color = this.selectColor(process.id);
  }

  override get title() {
    return `PID: ${this.process.pid.pid}`;
  }

  override get defaultDescriberName() {
    return 'ProcessNodeImpl';
  }

  override get targetYPositionStrength() {
    // Gravitate strongly towards the bottom of the graph. Can be overridden by
    // the bounding force which uses kMaxBoundaryStrength.
    return kHighYStrength;
  }

  override get linkStrengthScalingFactor() {
    // Give links to process nodes less weight than links between frame nodes,
    // so the that Y forces pulling process nodes into their area can dominate
    // over link forces pulling them towards frame nodes.
    return 0.5;
  }

  override allowedRangeY(graphHeight: number): [number, number] {
    return [graphHeight - kProcessNodesYRange, graphHeight];
  }

  override get manyBodyStrength() {
    return -600;
  }
}

class WorkerNode extends GraphNode {
  worker: WorkerInfo;

  constructor(worker: WorkerInfo) {
    super(worker.id);
    this.worker = worker;

    this.color = this.selectColor(worker.processId);
  }

  override get title() {
    return this.worker.url.length > 0 ? this.worker.url : 'Worker';
  }

  override get defaultDescriberName() {
    return 'WorkerNodeImpl';
  }

  override get targetYPositionStrength() {
    // Gravitate strongly towards the worker area of the graph. Can be
    // overridden by the bounding force which uses kMaxBoundaryStrength.
    return kHighYStrength;
  }

  override allowedRangeY(graphHeight: number): [number, number] {
    return [
      graphHeight - kWorkerNodesYRange,
      graphHeight - kProcessNodesYRange,
    ];
  }

  override get manyBodyStrength() {
    return -600;
  }

  override get linkTargets() {
    // Link the process, in addition to all the client and child workers.
    return [
      this.worker.processId,
      ...this.worker.clientFrameIds,
      ...this.worker.clientWorkerIds,
      ...this.worker.childWorkerIds,
    ];
  }
}

/**
 * A force that bounds GraphNodes |allowedRangeY| in Y,
 * as well as bounding them to stay in page bounds in X.
 */
function boundingForce(graphHeight: number, graphWidth: number) {
  let nodes: GraphNode[] = [];
  let bounds: Array<[number, number]> = [];
  const xBounds: [number, number] =
      [2 * kNodeRadius, graphWidth - 2 * kNodeRadius];
  const boundPosition = (pos: number, bound: [number, number]) =>
      Math.max(bound[0], Math.min(pos, bound[1]));

  function force(_alpha: number) {
    const n = nodes.length;
    for (let i = 0; i < n; ++i) {
      const bound = bounds[i];
      const node = nodes[i];
      assert(bound);
      assert(node);

      // Calculate where the node will end up after movement. If it will be out
      // of bounds apply a counter-force to bring it back in.
      const yNextPosition = node.y + node.vy!;
      const yBoundedPosition = boundPosition(yNextPosition, bound);
      if (yNextPosition !== yBoundedPosition) {
        // Do not include alpha because we want to be strongly repelled from
        // the boundary even if alpha has decayed.
        node.vy! += (yBoundedPosition - yNextPosition) * kMaxBoundaryStrength;
      }

      const xNextPosition = node.x + node.vx!;
      const xBoundedPosition = boundPosition(xNextPosition, xBounds);
      if (xNextPosition !== xBoundedPosition) {
        // Do not include alpha because we want to be strongly repelled from
        // the boundary even if alpha has decayed.
        node.vx! += (xBoundedPosition - xNextPosition) * kMaxBoundaryStrength;
      }
    }
  }

  force.initialize = function(n: GraphNode[]) {
    nodes = n;
    bounds = nodes.map(node => {
      const nodeBounds = node.allowedRangeY(graphHeight);
      // Leave space for the node circle plus a small border.
      nodeBounds[0] += kNodeRadius * 2;
      nodeBounds[1] -= kNodeRadius * 2;
      return nodeBounds;
    });
  };

  return force;
}

export class Graph implements GraphChangeStreamInterface {
  private svg_: SVGElement;
  private div_: Element;
  private wasResized_: boolean = false;
  private width_: number = 0;
  private height_: number = 0;
  private simulation_: d3.Simulation<GraphNode, undefined>|null = null;
  /** A selection for the top-level <g> node that contains all tooltip links. */
  private toolTipLinkGroup_:
      d3.Selection<SVGGElement, unknown, null, undefined>|null = null;
  /** A selection for the top-level <g> node that contains all separators. */
  private separatorGroup_: d3.Selection<SVGGElement, unknown, null, undefined>|
      null = null;
  /** A selection for the top-level <g> node that contains all nodes. */
  private nodeGroup_: d3.Selection<SVGGElement, unknown, null, undefined>|null =
      null;
  /** A selection for the top-level <g> node that contains all edges. */
  private linkGroup_: d3.Selection<
      SVGGElement, d3.SimulationLinkDatum<GraphNode>, null, undefined>|null =
      null;
  /** A selection for the top-level <g> node that contains all dashed edges. */
  private dashedLinkGroup_: d3.Selection<
      SVGGElement, d3.SimulationLinkDatum<GraphNode>, null, undefined>|null =
      null;
  private nodes_: Map<bigint, GraphNode> = new Map();
  private links_: Array<d3.SimulationLinkDatum<GraphNode>> = [];
  private dashedLinks_: Array<d3.SimulationLinkDatum<GraphNode>> = [];
  /** The interval timer used to poll for node descriptions. */
  private pollDescriptionsInterval_: number = 0;
  /** The d3.drag instance applied to nodes. */
  private drag_: d3.DragBehavior<SVGGElement, GraphNode, unknown>|null = null;

  constructor(svg: SVGElement, div: Element) {
    this.svg_ = svg;
    this.div_ = div;
  }

  initialize() {

    // Create the simulation and set up the permanent forces.
    const simulation: d3.Simulation<GraphNode, undefined> =
        d3.forceSimulation();
    simulation.on('tick', this.onTick_.bind(this));

    const linkForce =
        d3.forceLink<GraphNode, d3.SimulationLinkDatum<GraphNode>>().id(
            d => d.id.toString());
    const defaultStrength = linkForce.strength();

    // Override the default link strength function to apply scaling factors
    // from the source and target nodes to the link strength. This lets
    // different node types balance link forces with other forces that act on
    // them.
    simulation.force(
        'link',
        linkForce.strength(
            (l, i, n) => defaultStrength(l, i, n) *
                (l.source as GraphNode).linkStrengthScalingFactor *
                (l.target as GraphNode).linkStrengthScalingFactor));

    // Sets the repulsion force between nodes (positive number is attraction,
    // negative number is repulsion).
    simulation.force(
        'charge',
        d3.forceManyBody<GraphNode>().strength(
            this.getManyBodyStrength_.bind(this)));

    this.simulation_ = simulation;

    // Create the <g> elements that host nodes and links.
    // The link groups are created first so that all links end up behind nodes.
    const svg = d3.select(this.svg_);
    this.toolTipLinkGroup_ = svg.append('g').attr('class', 'tool-tip-links');
    this.linkGroup_ =
        svg.append('g').attr('class', 'links') as d3.Selection<
            SVGGElement, d3.SimulationLinkDatum<GraphNode>, null, undefined>;
    this.dashedLinkGroup_ =
        svg.append('g').attr('class', 'dashed-links') as d3.Selection<
            SVGGElement, d3.SimulationLinkDatum<GraphNode>, null, undefined>;
    this.nodeGroup_ = svg.append('g').attr('class', 'nodes');
    this.separatorGroup_ = svg.append('g').attr('class', 'separators');

    const drag = d3.drag() as d3.DragBehavior<SVGGElement, GraphNode, unknown>;
    drag.clickDistance(4);
    drag.on('start', this.onDragStart_.bind(this));
    drag.on('drag', this.onDrag_.bind(this));
    drag.on('end', this.onDragEnd_.bind(this));
    this.drag_ = drag;
  }

  frameCreated(frame: FrameInfo) {
    this.addNode_(new FrameNode(frame));
    this.render_();
  }

  pageCreated(page: PageInfo) {
    this.addNode_(new PageNode(page));
    this.render_();
  }

  processCreated(process: ProcessInfo) {
    this.addNode_(new ProcessNode(process));
    this.render_();
  }

  workerCreated(worker: WorkerInfo) {
    this.addNode_(new WorkerNode(worker));
    this.render_();
  }

  frameChanged(frame: FrameInfo) {
    const frameNode = this.nodes_.get(frame.id) as FrameNode;
    frameNode.frame = frame;
    this.render_();
  }

  pageChanged(page: PageInfo) {
    const pageNode = this.nodes_.get(page.id) as PageNode;

    // Page node dashed links may change dynamically, so account for that here.
    this.removeDashedNodeLinks_(pageNode);
    pageNode.page = page;
    this.addDashedNodeLinks_(pageNode);
    this.render_();
  }

  processChanged(process: ProcessInfo) {
    const processNode = this.nodes_.get(process.id) as ProcessNode;
    processNode.process = process;
    this.render_();
  }

  workerChanged(worker: WorkerInfo) {
    const workerNode = this.nodes_.get(worker.id) as WorkerNode;

    // Worker node links may change dynamically, so account for that here.
    this.removeNodeLinks_(workerNode);
    workerNode.worker = worker;
    this.addNodeLinks_(workerNode);
    this.render_();
  }

  favIconDataAvailable(iconInfo: FavIconInfo) {
    const graphNode = this.nodes_.get(iconInfo.nodeId);
    if (graphNode) {
      graphNode.iconUrl = 'data:image/png;base64,' + iconInfo.iconData;
    }
    this.render_();
  }

  nodeDeleted(nodeId: bigint) {
    const node = this.nodes_.get(nodeId)!;

    if (node.tooltip) {
      this.closeToolTip(node);
    }

    // Remove any links, and then the node itself.
    this.removeNodeLinks_(node);
    this.removeDashedNodeLinks_(node);
    this.nodes_.delete(nodeId);
    this.render_();
  }

  nodeDescriptions(nodeDescriptions: Map<bigint, string>) {
    for (const [nodeId, nodeDescription] of nodeDescriptions) {
      const node = this.nodes_.get(nodeId);
      if (node && node.tooltip) {
        node.tooltip.onDescription(nodeDescription);
      }
    }
    this.render_();
  }

  /** Updates floating tooltip positions as well as links to pinned tooltips */
  updateToolTipLinks() {
    const pinnedTooltips = [];
    for (const node of this.nodes_.values()) {
      const tooltip = node.tooltip;

      if (tooltip) {
        if (tooltip.floating) {
          tooltip.nodeMoved();
        } else {
          pinnedTooltips.push(tooltip);
        }
      }
    }

    function setLineEndpoints(
        d: ToolTip,
        line: d3.Selection<SVGLineElement, unknown, null, unknown>) {
      const center = d.getCenter();
      line.attr('x1', _d => center[0])
          .attr('y1', _d => center[1])
          .attr('x2', d => (d as {node: {x: number, y: number}}).node.x)
          .attr('y2', d => (d as {node: {x: number, y: number}}).node.y);
    }

    const toolTipLinks =
        this.toolTipLinkGroup_!.selectAll('line').data(pinnedTooltips);
    toolTipLinks.enter()
        .append('line')
        .attr('stroke', 'LightGray')
        .attr('stroke-dasharray', '1')
        .attr('stroke-opacity', '0.8')
        .each(function(d: ToolTip) {
          const line = d3.select(this);
          setLineEndpoints(d, line);
        });
    toolTipLinks.each(function(d: ToolTip) {
      const line = d3.select(this as SVGLineElement);
      setLineEndpoints(d, line);
    });
    toolTipLinks.exit().remove();
  }

  private removeNodeLinks_(node: GraphNode) {
    // Filter away any links to or from the provided node.
    this.links_ = this.links_.filter(
        link => link.source !== node && link.target !== node);
  }

  private removeDashedNodeLinks_(node: GraphNode) {
    // Filter away any dashed links to or from the provided node.
    this.dashedLinks_ = this.dashedLinks_.filter(
        link => link.source !== node && link.target !== node);
  }

  private pollForNodeDescriptions_() {
    const nodeIds: bigint[] = [];
    for (const node of this.nodes_.values()) {
      if (node.tooltip) {
        nodeIds.push(node.id);
      }
    }

    if (nodeIds.length) {
      this.div_.dispatchEvent(new CustomEvent('request-node-descriptions',
                                              { bubbles: true,
                                                composed: true,
                                                detail: nodeIds }));
      if (this.pollDescriptionsInterval_ === 0) {
        // Start polling if not already in progress.
        this.pollDescriptionsInterval_ =
            setInterval(this.pollForNodeDescriptions_.bind(this), 1000);
      }
    } else {
      // No tooltips, stop polling.
      clearInterval(this.pollDescriptionsInterval_);
      this.pollDescriptionsInterval_ = 0;
    }
  }

  closeToolTip(node: GraphNode) {
    if (node.tooltip) {
      node.tooltip.goAway();
      node.tooltip = null;
      this.updateToolTipLinks();
      this.pollForNodeDescriptions_();
    }
  }

  private onGraphNodeClick_(_event: MouseEvent, node: GraphNode) {
    if (node.tooltip) {
      this.closeToolTip(node);
    } else {
      node.tooltip = new ToolTip(this.div_, node, this);

      // Poll for all tooltip node descriptions immediately.
      this.pollForNodeDescriptions_();
    }
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
   */
  private render_() {
    // Select the links.
    const link = this.linkGroup_!.selectAll('line').data(this.links_);
    // Add new links.
    link.enter().append('line');
    // Remove dead links.
    link.exit().remove();

    // Select the dashed links.
    const dashedLink =
        this.dashedLinkGroup_!.selectAll('line').data(this.dashedLinks_);
    // Add new dashed links.
    dashedLink.enter().append('line');
    // Remove dead dashed links.
    dashedLink.exit().remove();

    // Select the nodes, except for any dead ones that are still transitioning.
    const nodes = Array.from(this.nodes_.values());
    const node =
        this.nodeGroup_!.selectAll<SVGGElement, GraphNode>('g:not(.dead)')
            .data(nodes, d => d.id as unknown as number);

    // Add new nodes, if any.
    if (!node.enter().empty()) {
      const newNodes = node.enter()
                           .append('g')
                           .call(this.drag_!)
                           .on('click', this.onGraphNodeClick_.bind(this));
      const circles = newNodes.append('circle')
                          .attr('id', d => `circle-${d.id}`)
                          .attr('r', kNodeRadius * 1.5)
                          .attr('fill', 'green');  // New nodes appear green.

      newNodes.append('image')
          .attr('x', -8)
          .attr('y', -8)
          .attr('width', 16)
          .attr('height', 16);
      newNodes.append('title');

      // Transition new nodes to their chosen color in 2 seconds.
      circles.transition()
          .duration(2000)
          .attr('fill', (d: unknown) => (d as {color: string}).color)
          .attr('r', kNodeRadius);
    }

    if (!node.exit().empty()) {
      // Give dead nodes a distinguishing class to exclude them from the
      // selection above.
      const deletedNodes = node.exit().classed('dead', true) as
          d3.Selection<SVGGElement, GraphNode, SVGGElement, unknown>;

      // Interrupt any ongoing transitions.
      deletedNodes.interrupt();

      // Turn down the node associated tooltips.
      deletedNodes.each(d => {
        if (d.tooltip) {
          d.tooltip.goAway();
        }
      });

      // Transition the nodes out and remove them at the end of transition.
      deletedNodes.transition()
          .remove()
          .select('circle')
          .attr('r', 9)
          .attr('fill', 'red')
          .transition()
          .duration(2000)
          .attr('r', 0);
    }

    // Update the title for all nodes.
    (node.selectAll('title') as
     d3.Selection<SVGElement, GraphNode, SVGGElement, unknown>)
        .text(d => d.title);
    // Update the favicon for all nodes.
    (node.selectAll('image') as
     d3.Selection<SVGImageElement, GraphNode, SVGGElement, unknown>)
        .attr('href', d => d.iconUrl);

    // Update and restart the simulation if the graph changed.
    if (!node.enter().empty() || !node.exit().empty() ||
        !link.enter().empty() || !link.exit().empty() ||
        !dashedLink.enter().empty() || !dashedLink.exit().empty()) {
      this.simulation_!.nodes(nodes);
      const links = this.links_.concat(this.dashedLinks_);
      this.simulation_!
          .force<d3.ForceLink<GraphNode, d3.SimulationLinkDatum<GraphNode>>>(
              'link')!.links(links);

      this.restartSimulation_();
    }
  }

  private onTick_() {
    const nodes: d3.Selection<SVGGElement, GraphNode, SVGGElement, unknown> =
        this.nodeGroup_!.selectAll('g');
    nodes.attr('transform', d => `translate(${d.x},${d.y})`);

    const lines: d3.Selection<
        SVGLineElement, d3.SimulationLinkDatum<GraphNode>, SVGGElement,
        d3.SimulationLinkDatum<GraphNode>> = this.linkGroup_!.selectAll('line');
    lines.attr('x1', d => (d.source as GraphNode).x)
        .attr('y1', d => (d.source as GraphNode).y)
        .attr('x2', d => (d.target as GraphNode).x)
        .attr('y2', d => (d.target as GraphNode).y);

    const dashedLines: d3.Selection<
        SVGLineElement, d3.SimulationLinkDatum<GraphNode>, SVGGElement,
        d3.SimulationLinkDatum<GraphNode>> =
        this.dashedLinkGroup_!.selectAll('line');
    dashedLines.attr('x1', d => (d.source as GraphNode).x)
        .attr('y1', d => (d.source as GraphNode).y)
        .attr('x2', d => (d.target as GraphNode).x)
        .attr('y2', d => (d.target as GraphNode).y);

    this.updateToolTipLinks();
  }

  /**
   * Adds a new node to the graph, populates its links and gives it an initial
   * position.
   */
  private addNode_(node: GraphNode) {
    this.nodes_.set(node.id, node);
    this.addNodeLinks_(node);
    this.addDashedNodeLinks_(node);
    node.setInitialPosition(this.width_, this.height_);
  }

  /**
   * Adds all the links for a node to the graph.
   */
  private addNodeLinks_(node: GraphNode) {
    for (const linkTarget of node.linkTargets) {
      const target = this.nodes_.get(linkTarget);
      if (target) {
        this.links_.push({source: node, target: target});
      }
    }
  }

  /**
   * Adds all the dashed links for a node to the graph.
   */
  private addDashedNodeLinks_(node: GraphNode) {
    for (const dashedLinkTarget of node.dashedLinkTargets) {
      const target = this.nodes_.get(dashedLinkTarget);
      if (target) {
        this.dashedLinks_.push({source: node, target: target});
      }
    }
  }

  /**
   * @param d The dragged node.
   */
  private onDragStart_(
      event: {active: number, x: number, y: number}, d: GraphNode) {
    if (!event.active) {
      this.restartSimulation_();
    }
    d.fx = d.x;
    d.fy = d.y;
  }

  /**
   * @param d The dragged node.
   */
  private onDrag_(event: {x: number, y: number}, d: GraphNode) {
    d.fx = event.x;
    d.fy = event.y;
  }

  /**
   * @param d The dragged node.
   */
  private onDragEnd_(
      event: {active: number, x: number, y: number}, d: GraphNode) {
    if (!event.active) {
      this.simulation_!.alphaTarget(0);
    }
    // Leave the node pinned where it was dropped. Return it to free
    // positioning if it's dropped outside its designated area.
    const bounds = d.allowedRangeY(this.height_);
    if (event.y < bounds[0] || event.y > bounds[1]) {
      d.fx = null;
      d.fy = null;
    }

    // Toggle the pinned class as appropriate for the circle backing this node.
    d3.select(`#circle-${d.id}`).classed('pinned', d.fx != null);
  }

  private getTargetPositionY_(d: GraphNode): number {
    return d.targetPositionY(this.height_);
  }

  private getTargetPositionStrengthY_(d: GraphNode): number {
    return d.targetYPositionStrength;
  }

  private getManyBodyStrength_(d: GraphNode): number {
    return d.manyBodyStrength;
  }

  /**
   * @param graphWidth Width of the graph view (svg).
   * @param graphHeight Height of the graph view (svg).
   */
  private updateSeparators_(graphWidth: number, graphHeight: number) {
    const separators = [
      ['Pages', 'Frame Tree', kPageNodesYRange],
      ['', 'Workers', graphHeight - kWorkerNodesYRange],
      ['', 'Processes', graphHeight - kProcessNodesYRange],
    ];
    const kAboveLabelOffset = -6;
    const kBelowLabelOffset = 14;

    const groups = this.separatorGroup_!.selectAll('g').data(separators);
    if (groups.enter()) {
      const group = groups.enter().append('g').attr(
          'transform', (d: Array<number|string>) => `translate(0,${d[2]})`);
      group.append('line')
          .attr('x1', 10)
          .attr('y1', 0)
          .attr('x2', graphWidth - 10)
          .attr('y2', 0)
          .attr('stroke', 'black')
          .attr('stroke-dasharray', '4');

      group.each(function(d: unknown) {
        const parentGroup = d3.select(this);
        const aboveLabel = (d as Array<string|number>)[0];
        const belowLabel = (d as Array<string|number>)[1];
        if (aboveLabel) {
          parentGroup.append('text')
              .attr('x', 20)
              .attr('y', kAboveLabelOffset)
              .attr('class', 'separator')
              .text(aboveLabel);
        }
        if (belowLabel) {
          parentGroup.append('text')
              .attr('x', 20)
              .attr('y', kBelowLabelOffset)
              .attr('class', 'separator')
              .text(belowLabel);
        }
      });
    }

    groups.attr('transform', (d: unknown) => {
      const value = (d as Array<string|number>)[2];
      return `translate(0,${value})`;
    });
    groups.selectAll('line').attr('x2', graphWidth - 10);
  }

  private restartSimulation_() {
    // Restart the simulation.
    this.simulation_!.alphaTarget(0.3).restart();
  }

  /**
   * Resizes and restarts the animation after a size change.
   */
  onResize() {
    this.width_ = this.svg_.clientWidth;
    this.height_ = this.svg_.clientHeight;

    this.updateSeparators_(this.width_, this.height_);

    // Reset both X and Y attractive forces, as they're cached.
    const xForce = d3.forceX().x(this.width_ / 2).strength(0.1);
    const yForce = d3.forceY<GraphNode>()
                       .y(this.getTargetPositionY_.bind(this))
                       .strength(this.getTargetPositionStrengthY_.bind(this));
    this.simulation_!.force('x_pos', xForce);
    this.simulation_!.force('y_pos', yForce);
    this.simulation_!.force(
        'y_bound', boundingForce(this.height_, this.width_));

    if (!this.wasResized_) {
      this.wasResized_ = true;

      // Reinitialize all node positions on first resize.
      this.nodes_.forEach(
          node => node.setInitialPosition(this.width_, this.height_));

      // Allow the simulation to settle by running it for a bit.
      for (let i = 0; i < 200; ++i) {
        this.simulation_!.tick();
      }
    }

    this.restartSimulation_();
  }
}
