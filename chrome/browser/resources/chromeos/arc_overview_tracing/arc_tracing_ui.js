// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI.
 */

// Namespace of SVG elements
const svgNS = 'http://www.w3.org/2000/svg';

// Background color for the band with events.
const bandColor = '#d3d3d3';

// Color that should never appear on UI.
const unusedColor = '#ff0000';

// Supported zooms, mcs per pixel
const zooms = [
  2.5,
  5.0,
  10.0,
  25.0,
  50.0,
  100.0,
  250.0,
  500.0,
  1000.0,
  2500.0,
  5000.0,
  10000.0,
  25000.0,
];

// Active zoom level, as index in |zooms|. By default 100 mcs per pixel.
let zoomLevel = 5;

// Graphics event types which are used in the model JSON data. These must match
// the graphics event types in
// chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h. To aid in
// maintaining consistency, do not modify values once added - deprecation and
// removal are allowed.
const kExoSurfaceCommit = 206;
const kExoSurfaceCommitJank = 207;
const kChromeOSPresentationDone = 503;
const kChromeOSSwapDone = 504;
const kChromeOSPerceivedJank = 506;
const kChromeOSSwapJank = 507;

/**
 * Keep in sync with ArcTracingGraphicsModel::EventType
 * See chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h.
 * Describes how events should be rendered. |color| specifies color of the
 * event, |name| is used in tooltips. |width| defines the width in case it is
 * rendered as a line and |radius| defines the radius in case it is rendered as
 * a circle.
 *
 * TODO(matvore): Only kIdleIn and kIdleOut are used in bands. Verify and clean
 * up.
 */
const eventAttributes = {
  // kIdleIn
  0: {color: bandColor, name: 'idle'},
  // kIdleOut
  1: {color: '#ffbf00', name: 'active'},

  // kBufferQueueDequeueStart
  100: {color: '#99cc00', name: 'app requests buffer'},
  // kBufferQueueDequeueDone
  101: {color: '#669999', name: 'app fills buffer'},
  // kBufferQueueQueueStart
  102: {color: '#cccc00', name: 'app queues buffer'},
  // kBufferQueueQueueDone
  103: {color: unusedColor, name: 'buffer is queued'},
  // kBufferQueueAcquire
  104: {color: '#66ffcc', name: 'use buffer'},
  // kBufferQueueReleased
  105: {color: unusedColor, name: 'buffer released'},
  // kBufferFillJank
  106: {color: '#ff0000', name: 'buffer filling jank', width: 1.0, radius: 4.0},

  [kExoSurfaceCommitJank]: {name: 'commit jank', radius: 4.0},

  // kChromeBarrierOrder.
  300: {color: '#ff9933', name: 'barrier order'},
  // kChromeBarrierFlush
  301: {color: unusedColor, name: 'barrier flush'},

  // kSurfaceFlingerInvalidationStart
  401: {color: '#ff9933', name: 'invalidation start'},
  // kSurfaceFlingerInvalidationDone
  402: {color: unusedColor, name: 'invalidation done'},
  // kSurfaceFlingerCompositionStart
  403: {color: '#3399ff', name: 'composition start'},
  // kSurfaceFlingerCompositionDone
  404: {color: unusedColor, name: 'composition done'},

  // kChromeOSDraw
  500: {color: '#3399ff', name: 'draw'},
  // kChromeOSSwap
  501: {color: '#cc9900', name: 'swap'},
  // kChromeOSWaitForAck
  502: {color: '#ccffff', name: 'wait for ack'},
  // kChromeOSPresentationDone
  503: {color: '#ffbf00', name: 'presentation done'},
  // kChromeOSSwapDone
  504: {color: '#65f441', name: 'swap done'},
  // kChromeOSJank
  505: {
    color: '#ff0000',
    name: 'Chrome composition jank',
    width: 1.0,
    radius: 4.0,
  },
  [kChromeOSPerceivedJank]: {
    name: 'perceived jank',
    radius: 4.0,
  },
  [kChromeOSSwapJank]: {
    name: 'swap jank',
    radius: 4.0,
  },

  // kCustomEvent
  600: {color: '#7cb342', name: 'Custom event', width: 1.0, radius: 4.0},

  // Service events.
  // kTimeMark
  10000: {color: '#888', name: 'Time mark', width: 0.75},
  // kTimeMarkSmall
  10001: {color: '#888', name: 'Time mark', width: 0.15},
};

/**
 * Defines the map of events that can be treated as the end of event sequence.
 * Time after such events is considered as idle time until the next event
 * starts. Key of |endSequenceEvents| is event type as defined in
 * ArcTracingGraphicsModel::EventType and value is the list of event
 * types that should follow after the tested event to consider it as end of
 * sequence. Empty list means that tested event is certainly end of the
 * sequence.
 */
const endSequenceEvents = {
  // kIdleIn
  0: [],
  // kBufferQueueQueueDone
  103: [],
  // kBufferQueueReleased
  105: [],
  // kChromeBarrierFlush
  301: [],
  // kSurfaceFlingerInvalidationDone
  402: [],
  // kSurfaceFlingerCompositionDone
  404: [],
  // kChromeOSPresentationDone. Chrome does not define exactly which event
  // is the last. Different
  // pipelines may produce different sequences. Both event type may indicate
  // the end of the
  // sequence.
  503: [500 /* kChromeOSDraw */],
  // kChromeOSSwapDone
  504: [500 /* kChromeOSDraw */],
};

/**
 * Keep in sync with ArcValueEvent::Type
 * See chrome/browser/ash/arc/tracing/arc_value_event.h.
 * Describes how value events should be rendered in charts. |color| specifies
 * color of the event, |name| is used in tooltips, |width| specify width of
 * the line in chart, |scale| is used to convert actual value to rendered value.
 * When rendered, min and max values and determined and used as a range where
 * chart is drawn. However, in the case range is small, let say 1 mb for
 * |kMemUsed| this may lead to user confusion that huge amount of memory was
 * allocated. To prevent this scanario, |minRange| defines the minimum range of
 * values and is set in scaled units.
 */
const valueAttributes = {
  // kMemUsed.
  1: {
    color: '#ff3d00',
    minRange: 512.0,
    name: 'used mb',
    scale: 1.0 / 1024.0,
    width: 1.0,
  },
  // kSwapRead.
  2: {
    color: '#ffc400',
    minRange: 32.0,
    name: 'swap read sectors',
    scale: 1.0,
    width: 1.0,
  },
  // kSwapWrite.
  3: {
    color: '#ff9100',
    minRange: 32.0,
    name: 'swap write sectors',
    scale: 1.0,
    width: 1.0,
  },
  // kGemObjects.
  5: {
    color: '#3d5afe',
    minRange: 1000,
    name: 'geom. objects',
    scale: 1.0,
    width: 1.0,
  },
  // kGemSize.
  6: {
    color: '#7c4dff',
    minRange: 256.0,
    name: 'geom. size mb',
    scale: 1.0 / 1024.0,
    width: 1.0,
  },
  // kGpuFrequency.
  7: {
    color: '#01579b',
    minRange: 300.0,
    name: 'GPU frequency mhz',
    scale: 1.0,
    width: 1.0,
  },
  // kCpuTemperature.
  8: {
    color: '#ff3d00',
    minRange: 20.0,
    name: 'CPU celsius.',
    scale: 1.0 / 1000.0,
    width: 1.0,
  },
  // kCpuFrequency.
  9: {
    color: '#ff80ab',
    minRange: 300.0,
    name: 'CPU Mhz.',
    scale: 1.0 / 1000.0,
    width: 1.0,
  },
  // kCpuPower.
  10: {
    color: '#dd2c00',
    minRange: 0.0,
    name: 'CPU milli-watts.',
    scale: 1.0,
    width: 1.0,
  },
  // kGpuPower.
  11: {
    color: '#dd2c00',
    minRange: 0.0,
    name: 'GPU milli-watts.',
    scale: 1.0,
    width: 1.0,
  },
  // kMemoryPower.
  12: {
    color: '#dd2c00',
    minRange: 0.0,
    name: 'Memory milli-watts.',
    scale: 1.0,
    width: 1.0,
  },
  // kPackagePowerConstraint.
  13: {
    color: '#dd0050',
    minRange: 0.0,
    name: 'CPU package constraint milli-watts.',
    scale: 1.0,
    width: 1.0,
  },
};

/**
 * @type {function()}.
 * Callback when UI has to be updted.
 */
let updateUiCallback = null;

/**
 * @type {DetailedInfoView}.
 * Currently active detailed view.
 */
let activeDetailedInfoView = null;

/**
 * Discards active detailed view if it exists.
 */
function discardDetailedInfo() {
  if (activeDetailedInfoView) {
    activeDetailedInfoView.discard();
    activeDetailedInfoView = null;
  }
}

/**
 * Shows detailed view for |eventBand| in response to mouse click event
 * |mouseEvent|.
 */
function showDetailedInfoForBand(eventBand, mouseEvent) {
  discardDetailedInfo();
  activeDetailedInfoView = eventBand.showDetailedInfo(mouseEvent);
  mouseEvent.preventDefault();
}

/**
 * Returns text representation of timestamp in milliseconds with one number
 * after the decimal point.
 *
 * @param {number} timestamp in microseconds.
 */
function timestampToMsText(timestamp) {
  return (timestamp / 1000.0).toFixed(1);
}

/**
 * Changes zoom. |delta| specifies how many zoom levels to adjust. Negative
 * |delta| means zoom in and positive zoom out.
 */
function updateZoom(delta) {
  const newZoomLevel = zoomLevel + delta;
  if (newZoomLevel < 0 || newZoomLevel >= zooms.length) {
    return;
  }

  zoomLevel = newZoomLevel;
  updateUiCallback();
}

/**
 * Initialises common tracing UI.
 *  * handle zoom.
 *  * handle load request
 *  * set keyboard and mouse listeners to discard detailed view overlay.
 */
function initializeUi(initZoomLevel, callback) {
  zoomLevel = initZoomLevel;
  updateUiCallback = callback;

  document.body.onkeydown = function(event) {
    // Escape and Enter.
    if (event.key === 'Escape' || event.key === 'Enter') {
      discardDetailedInfo();
    } else if (event.key === 'w') {
      // Zoom in.
      updateZoom(-1 /* delta */);
    } else if (event.key === 's') {
      // Zoom out.
      updateZoom(1 /* delta */);
    }
  };

  window.onclick = function(event) {
    // Detect click outside the detailed view.
    if (event.defaultPrevented || activeDetailedInfoView == null) {
      return;
    }
    if (!activeDetailedInfoView.overlay.contains(event.target)) {
      discardDetailedInfo();
    }
  };

  if ($('arc-tracing-load')) {
    $('arc-tracing-load').onclick = function(event) {
      const fileElement = document.createElement('input');
      fileElement.type = 'file';

      fileElement.onchange = function(event) {
        const reader = new FileReader();
        reader.onload = function(response) {
          chrome.send('loadFromText', [response.target.result]);
        };
        reader.readAsText(event.target.files[0]);
      };

      fileElement.click();
    };
  }
}

/**
 * Updates current status.
 * @param {string} statusText text to set as a status.
 */
function setStatus(statusText) {
  $('arc-tracing-status').textContent = statusText;
}

/** Factory class for SVG elements. */
class SVG {
  // Creates rectangle element in the |svg| with provided attributes.
  static addRect(svg, x, y, width, height, color, opacity) {
    const rect = document.createElementNS(svgNS, 'rect');
    rect.setAttributeNS(null, 'x', x);
    rect.setAttributeNS(null, 'y', y);
    rect.setAttributeNS(null, 'width', width);
    rect.setAttributeNS(null, 'height', height);
    rect.setAttributeNS(null, 'fill', color);
    rect.setAttributeNS(null, 'stroke', 'none');
    if (opacity) {
      rect.setAttributeNS(null, 'fill-opacity', opacity);
    }
    svg.appendChild(rect);
    return rect;
  }

  // Creates line element in the |svg| with provided attributes.
  static addLine(svg, x1, y1, x2, y2, color, width) {
    const line = document.createElementNS(svgNS, 'line');
    line.setAttributeNS(null, 'x1', x1);
    line.setAttributeNS(null, 'y1', y1);
    line.setAttributeNS(null, 'x2', x2);
    line.setAttributeNS(null, 'y2', y2);
    line.setAttributeNS(null, 'stroke', color);
    line.setAttributeNS(null, 'stroke-width', width);
    svg.appendChild(line);
    return line;
  }

  // Creates polyline element in the |svg| with provided attributes.
  static addPolyline(svg, points, color, width) {
    const polyline = document.createElementNS(svgNS, 'polyline');
    polyline.setAttributeNS(null, 'points', points.join(' '));
    polyline.setAttributeNS(null, 'stroke', color);
    polyline.setAttributeNS(null, 'stroke-width', width);
    polyline.setAttributeNS(null, 'fill', 'none');
    svg.appendChild(polyline);
    return polyline;
  }

  // Creates circle element in the |svg| with provided attributes.
  static addCircle(svg, x, y, radius, strokeWidth, color, strokeColor) {
    const circle = document.createElementNS(svgNS, 'circle');
    circle.setAttributeNS(null, 'cx', x);
    circle.setAttributeNS(null, 'cy', y);
    circle.setAttributeNS(null, 'r', radius);
    circle.setAttributeNS(null, 'fill', color);
    circle.setAttributeNS(null, 'stroke', strokeColor);
    circle.setAttributeNS(null, 'stroke-width', strokeWidth);
    svg.appendChild(circle);
    return circle;
  }

  // Creates text element in the |svg| with provided attributes.
  static addText(svg, x, y, fontSize, textContent, anchor, transform) {
    const lines = textContent.split('\n');
    let text;
    for (let i = 0; i < lines.length; ++i) {
      text = document.createElementNS(svgNS, 'text');
      text.setAttributeNS(null, 'x', x);
      text.setAttributeNS(null, 'y', y);
      text.setAttributeNS(null, 'fill', 'black');
      text.setAttributeNS(null, 'font-size', fontSize);
      if (anchor) {
        text.setAttributeNS(null, 'text-anchor', anchor);
      }
      if (transform) {
        text.setAttributeNS(null, 'transform', transform);
      }
      text.appendChild(document.createTextNode(lines[i]));
      svg.appendChild(text);
      y += fontSize;
    }
    return text;
  }
}

/**
 * Represents title for events bands that can collapse/expand controlled
 * content.
 */
class EventBandTitle {
  constructor(parent, anchor, title, className, opt_iconContent) {
    this.div = document.createElement('div');
    this.div.classList.add(className);
    if (opt_iconContent) {
      const icon = document.createElement('img');
      icon.src = 'data:image/png;base64,' + opt_iconContent;
      this.div.appendChild(icon);
    }
    const span = document.createElement('span');
    span.appendChild(document.createTextNode(title));
    this.div.appendChild(span);
    this.controlledItems = [];
    this.div.onclick = this.onClick_.bind(this);
    this.parent = parent;
    if (anchor && anchor.nextSibling) {
      this.parent.insertBefore(this.div, anchor.nextSibling);
    } else {
      this.parent.appendChild(this.div);
    }
  }

  /**
   * Adds extra HTML element under the control. This element will be
   * automatically expanded/collapsed together with this title.
   *
   * @param {HTMLElement} item svg element to control.
   */
  addContolledItems(item) {
    this.controlledItems.push(item);
  }

  onClick_() {
    this.div.classList.toggle('hidden');
    for (let i = 0; i < this.controlledItems.length; ++i) {
      this.controlledItems[i].classList.toggle('hidden');
    }
  }
}

/** Represents container for event bands. */
class EventBands {
  /**
   * Creates container for the event bands.
   *
   * @param {EventBandTitle} title controls visibility of this band.
   * @param {string} className class name of the svg element that represents
   *     this band. 'arc-events-top-band' is used for top-level events and
   *     'arc-events-inner-band' is used for per-buffer events.
   * @param {number} resolution the resolution of bands microseconds per 1
   *     pixel.
   * @param {number} minTimestamp the minimum timestamp to display on bands.
   * @param {number} minTimestamp the maximum timestamp to display on bands.
   */
  constructor(title, className, resolution, minTimestamp, maxTimestamp) {
    // Keep information about bands and charts and their bounds.
    this.bands = [];
    this.charts = [];
    this.globalEvents = [];
    this.tooltips = [];
    this.resolution = resolution;
    this.minTimestamp = minTimestamp;
    this.maxTimestamp = maxTimestamp;
    this.height = 0;
    // Offset of the next band of events.
    this.nextYOffset = 0;
    this.svg = document.createElementNS(svgNS, 'svg');
    this.svg.setAttributeNS(
        'http://www.w3.org/2000/xmlns/', 'xmlns:xlink',
        'http://www.w3.org/1999/xlink');
    this.setBandOffsetX(0);
    this.setWidth(0);
    this.svg.setAttribute('height', this.height + 'px');
    this.svg.classList.add(className);

    this.setTooltip_();
    this.title = title;
    title.addContolledItems(this.svg);
    title.parent.insertBefore(this.svg, title.div.nextSibling);

    // Set of constants, used for rendering content.
    this.fontSize = 12;
    this.verticalGap = 5;
    this.horizontalGap = 10;
    this.lineHeight = 16;
    this.iconOffset = 24;
    this.iconRadius = 4;
    this.textOffset = 32;
  }

  /**
   * Sets the horizontal offset to render bands.
   * @param {number} offsetX offset in pixels.
   */
  setBandOffsetX(offsetX) {
    this.bandOffsetX = offsetX;
  }

  /**
   * Sets the widths of event bands.
   * @param {number} width width in pixels.
   */
  setWidth(width) {
    this.width = width;
    this.svg.setAttribute('width', this.width + 'px');
  }

  /**
   * Converts timestamp into pixel offset. 1 pixel corresponds resolution
   * microseconds.
   *
   * @param {number} timestamp in microseconds.
   */
  timestampToOffset(timestamp) {
    return (timestamp - this.minTimestamp) / this.resolution;
  }

  /**
   * Opposite conversion of |timestampToOffset|
   *
   * @param {number} offset in pixels.
   */
  offsetToTime(offset) {
    return offset * this.resolution + this.minTimestamp;
  }

  /**
   * This adds new band of events. Height of svg container is automatically
   * adjusted to fit the new content.
   *
   * @param {Events} eventBand event band to add.
   * @param {number} height of the band.
   * @param {number} padding to separate from the next band or chart.
   */
  addBand(eventBand, height, padding) {
    let currentColor = unusedColor;
    let addToBand = false;
    let x = this.bandOffsetX;
    let eventIndex = eventBand.getFirstAfter(this.minTimestamp);
    while (eventIndex >= 0) {
      const event = eventBand.events[eventIndex];
      if (event[1] >= this.maxTimestamp) {
        break;
      }
      const nextX = this.timestampToOffset(event[1]) + this.bandOffsetX;
      if (addToBand) {
        SVG.addRect(
            this.svg, x, this.nextYOffset, nextX - x, height, currentColor);
      }
      if (eventBand.isEndOfSequence(eventIndex)) {
        currentColor = unusedColor;
        addToBand = false;
      } else {
        currentColor = eventAttributes[event[0]].color;
        addToBand = true;
      }
      x = nextX;
      eventIndex = eventBand.getNextEvent(eventIndex, 1 /* direction */);
    }
    if (addToBand) {
      SVG.addRect(
          this.svg, x, this.nextYOffset,
          this.timestampToOffset(this.maxTimestamp) - x + this.bandOffsetX,
          height, currentColor);
    }

    this.bands.push({
      band: eventBand,
      top: this.nextYOffset,
      bottom: this.nextYOffset + height,
    });

    this.updateHeight(height, padding);
  }

  /**
   * This adds horizontal separator at |nextYOffset|.
   *
   * @param {number} padding to separate from the next band or chart.
   */
  addBandSeparator(padding) {
    SVG.addLine(
        this.svg, 0, this.nextYOffset, this.width, this.nextYOffset, '#888',
        0.25);
    this.updateHeight(0 /* height */, padding);
  }

  /**
   * This adds new chart. Height of svg container is automatically adjusted to
   * fit the new content. This creates empty chart and one or more calls
   * |addChartSources| are expected to add actual content to the chart.
   *
   * @param {number} height of the chart.
   * @param {number} padding to separate from the next band or chart.
   */
  addChart(height, padding) {
    this.charts.push({
      sourcesWithBounds: [],
      top: this.nextYOffset,
      bottom: this.nextYOffset + height,
    });

    this.updateHeight(height, padding);
  }

  /**
   * This adds new chart into existing area.
   * @param {number} top position of chart.
   * @param {number} bottom position of chart.
   */
  addChartToExistingArea(top, bottom) {
    this.charts.push({sourcesWithBounds: [], top: top, bottom: bottom});
  }


  /**
   * This adds sources of events to the last chart.
   *
   * @param {Events[]} sources is array of groupped source of events to add.
   *     These events are logically linked to each other and represented as a
   *     separate line.
   * @param {boolean} smooth if set to true then indicates that chart should
   *                  display value interpolated, otherwise values are changed
   *                  discretely.
   * @param {Object=} opt_attributes optional argument that defines view of
   *                  the chart. If not set then it is automatically selected
   *                  based on event type.
   */
  addChartSources(sources, smooth, opt_attributes) {
    const chart = this.charts[this.charts.length - 1];

    // Calculate min/max for sources and event indices.
    let minValue = null;
    let maxValue = null;
    const eventIndicesForAll = [];
    let eventDetected = false;
    let attributes = opt_attributes;
    const autoDetectRange = !attributes ||
        typeof attributes.minValue === 'undefined' ||
        typeof attributes.maxValue === 'undefined';
    for (let i = 0; i < sources.length; ++i) {
      const source = sources[i];
      let eventIndex = source.getFirstAfter(this.minTimestamp);
      if (eventIndex < 0 || source.events[eventIndex][1] > this.maxTimestamp) {
        eventIndicesForAll.push([]);
        continue;
      }
      if (autoDetectRange && !minValue) {
        minValue = source.events[eventIndex][2];
        maxValue = source.events[eventIndex][2];
      }
      const eventIndices = [];
      while (eventIndex >= 0 &&
             source.events[eventIndex][1] <= this.maxTimestamp) {
        eventIndices.push(eventIndex);
        eventDetected = true;
        if (autoDetectRange) {
          minValue = Math.min(minValue, source.events[eventIndex][2]);
          maxValue = Math.max(maxValue, source.events[eventIndex][2]);
        }
        if (!attributes) {
          attributes = valueAttributes[source.events[eventIndex][0]];
        }
        eventIndex = source.getNextEvent(eventIndex, 1 /* direction */);
      }
      eventIndicesForAll.push(eventIndices);
    }

    if (!eventDetected) {
      // no one event to render.
      return;
    }

    if (autoDetectRange) {
      // Ensure minimum value range.
      if (maxValue - minValue < attributes.minRange / attributes.scale) {
        maxValue = minValue + attributes.minRange / attributes.scale;
      }

      // Add +-1% to bounds.
      const dif = maxValue - minValue;
      minValue -= dif * 0.01;
      maxValue += dif * 0.01;
    } else {
      minValue = attributes.minValue;
      maxValue = attributes.maxValue;
    }
    const divider = 1.0 / (maxValue - minValue);

    // Render now.
    const height = chart.bottom - chart.top;
    for (let i = 0; i < sources.length; ++i) {
      const source = sources[i];
      const eventIndices = eventIndicesForAll[i];
      if (eventIndices.length === 0) {
        continue;
      }
      // Determine type using first element.
      const eventType = source.events[eventIndices[0]][0];

      const points = [];
      let lastY = 0;
      for (let j = 0; j < eventIndices.length; ++j) {
        const event = source.events[eventIndices[j]];
        const x = this.timestampToOffset(event[1]);
        const y = height * (maxValue - event[2]) * divider;
        if (!smooth && j !== 0) {
          points.push([x, lastY]);
        }
        points.push([x, y]);
        lastY = y;
      }

      chart.sourcesWithBounds.push({
        attributes: attributes,
        minValue: minValue,
        maxValue: maxValue,
        source: source,
        smooth: smooth,
      });

      SVG.addPolyline(this.svg, points, attributes.color, attributes.width);
    }
  }

  /**
   * This adds text to chart.
   *
   * @param {string} text to display.
   * @param {number} x horizontal position in the chart.
   * @param {number} y vertical position the chart.
   * @param {string} anchor align of the text relative to x.
   */
  addChartText(text, x, y, anchor) {
    SVG.addText(this.svg, x, y, this.fontSize, text, anchor);
  }

  /**
   * This adds bar as rectangle to the chart.
   *
   * @param {number} x horizontal position in the chart.
   * @param {number} y vertical position in the chart.
   * @param {number} width horizontal dimension of the bar.
   * @param {number} height vertical dimension of the bar.
   * @param {string} color of the bar.
   */
  addChartBar(x, y, width, height, color) {
    SVG.addRect(this.svg, x, y, width, height, color, 1.0 /* opacity */);
  }

  /**
   * This adds popup tooltip for the fixed area in the chart.
   *
   * @param {number} x horizontal position of the tooltip area in the chart.
   * @param {number} y vertical position of the tooltip area in the chart.
   * @param {number} width of the tooltip area.
   * @param {number} height of the tooltip area.
   * @param {string} tooltip text to display.
   * @param {number} tooltipWidth of the tooltip window.
   * @param {number} tooltipHeight of the tooltip window.
   */
  addChartTooltip(x, y, width, height, tooltip, tooltipWidth, tooltipHeight) {
    this.tooltips.push({
      left: x,
      top: y,
      right: x + width - 1,
      bottom: y + height - 1,
      tooltip: tooltip,
      tooltipWidth: tooltipWidth,
      tooltipHeight: tooltipHeight,
    });
  }

  /**
   * This adds HTML input element to the title of the section.
   *
   * @param {string} type specifies input type, like radio.
   * @param {string} text label for the input.
   * @param {boolean} checked if radio should be initially checked.
   * @param {function} handler callback for input change.
   */
  createTitleInput(type, text, checked, handler) {
    const input = document.createElement('input');
    input.onclick = handler;
    input.setAttribute('type', type);
    if (type === 'button') {
      input.setAttribute('value', text);
    }
    if (checked) {
      input.checked = true;
    }
    this.title.addContolledItems(input);
    this.title.div.appendChild(input);
    if (type === 'button') {
      return;
    }
    const label = document.createElement('label');
    label.onclick = handler;
    label.appendChild(document.createTextNode(text));
    this.title.addContolledItems(label);
    this.title.div.appendChild(label);
  }

  /**
   * This adds sources of events to the last chart as a bars.
   *
   * @param {Events[]} sources is array of groupped source of events to add.
   *     These events are logically linked to each other and represented as a
   *     bar where each bar has color corresponded the value of the event.
   * @param {Object=} attributes dictionary to resolve the color of the bar.
   * @param {number} y vertical offset of bars.
   * @param {number} height height of bars.
   */
  addBarSource(source, attributes, y, height) {
    const chart = this.charts[this.charts.length - 1];

    let eventIndex = source.getFirstAfter(this.minTimestamp);
    if (eventIndex < 0 || source.events[eventIndex][1] > this.maxTimestamp) {
      return;
    }

    while (eventIndex >= 0 &&
           source.events[eventIndex][1] <= this.maxTimestamp) {
      const eventIndexNext = source.getNextEvent(eventIndex, 1 /* direction */);
      const event = source.events[eventIndex];
      const x = this.timestampToOffset(event[1]);
      const color = attributes[event[2]].color;
      let nextTimestamp = 0;
      if (eventIndexNext >= 0 &&
          source.events[eventIndexNext][1] <= this.maxTimestamp) {
        nextTimestamp = source.events[eventIndexNext][1];
      } else {
        nextTimestamp = this.maxTimestamp;
      }
      const width = this.timestampToOffset(nextTimestamp);
      eventIndex = eventIndexNext;
      SVG.addRect(this.svg, x, y, width, height, color, 1.0 /* opacity */);
    }
    chart.sourcesWithBounds.push(
        {attributes: attributes, source: source, perValue: true});
  }

  addChartGridLine(y) {
    SVG.addLine(this.svg, 0, y, this.width, y, '#ccc', 0.5);
  }

  /**
   * Updates height of svg container.
   *
   * @param {number} new height of the chart.
   * @param {number} padding to separate from the next band or chart.
   */
  updateHeight(height, padding) {
    this.nextYOffset += height;
    this.height = this.nextYOffset;
    this.svg.setAttribute('height', this.height + 'px');
    this.nextYOffset += padding;
  }

  /**
   * This adds events as a global events that do not belong to any band.
   *
   * @param {Events} events to add.
   * @param {string} renderType defines how to render events, can be underfined
   *                 for default or set to 'circle'.
   * @param {string} color the color (fill color if rendered as a circle), or
   *                 omitted to use the color defined in eventAttributes for
   *                 each event type.
   * @param {number} y for circles, the y position, as a fraction of this band's
   *                 height, such as 0.5 for vertically-centered, or 0.95 for
   *                 close to the bottom.
   */
  addGlobal(events, renderType, color, y) {
    let eventIndex = -1;
    if (typeof y === 'undefined') {
      y = 0.5;
    }
    while (true) {
      eventIndex = events.getNextEvent(eventIndex, 1 /* direction */);
      if (eventIndex < 0) {
        break;
      }
      const event = events.events[eventIndex];
      const attributes = events.getEventAttributes(eventIndex);
      const x = this.timestampToOffset(event[1]) + this.bandOffsetX;
      const evColor = color || attributes.color;
      if (renderType === 'circle') {
        SVG.addCircle(
            this.svg, x, this.height * y, attributes.radius,
            1 /* strokeWidth */, evColor, 'black' /* strokeColor */);
      } else {
        SVG.addLine(this.svg, x, 0, x, this.height, evColor, attributes.width);
      }
    }
    this.globalEvents.push(events);
  }

  /** Initializes tooltip support by observing mouse events */
  setTooltip_() {
    this.tooltip = $('arc-event-band-tooltip');
    this.svg.onmouseover = this.showToolTip_.bind(this);
    this.svg.onmouseout = this.hideToolTip_.bind(this);
    this.svg.onmousemove = this.updateToolTip_.bind(this);
    this.svg.onclick = (event) => {
      showDetailedInfoForBand(this, event);
    };
  }

  /** Updates tooltip and shows it for this band. */
  showToolTip_(event) {
    this.updateToolTip_(event);
  }

  /** Hides the tooltip. */
  hideToolTip_() {
    this.tooltip.classList.remove('active');
  }

  /**
   * Finds global events around |timestamp| and not farther than |distance|.
   *
   * @param {number} timestamp to search.
   * @param {number} distance to search.
   * @returns {Array} array of events sorted by distance to |timestamp| from
   *                  closest to farthest.
   */
  findGlobalEvents_(timestamp, distance) {
    const events = [];
    const leftBorder = timestamp - distance;
    const rightBorder = timestamp + distance;
    for (let i = 0; i < this.globalEvents.length; ++i) {
      let index = this.globalEvents[i].getFirstAfter(leftBorder);
      while (index >= 0) {
        const event = this.globalEvents[i].events[index];
        if (event[1] > rightBorder) {
          break;
        }
        events.push(event);
        index = this.globalEvents[i].getNextEvent(index, 1 /* direction */);
      }
    }
    events.sort(function(a, b) {
      const distanceA = Math.abs(timestamp - a[1]);
      const distanceB = Math.abs(timestamp - b[1]);
      return distanceA - distanceB;
    });
    return events;
  }

  /**
   * Updates tool tip based on event under the current cursor.
   *
   * @param {Object} event mouse event.
   */
  updateToolTip_(event) {
    // Clear previous content.
    this.tooltip.textContent = '';

    const svgStyle = window.getComputedStyle(this.svg, null);
    const paddingLeft = parseFloat(svgStyle.getPropertyValue('padding-left'));
    const paddingTop = parseFloat(svgStyle.getPropertyValue('padding-top'));
    const eventX = event.offsetX - paddingLeft;
    const eventY = event.offsetY - paddingTop;

    const svg = document.createElementNS(svgNS, 'svg');
    svg.setAttributeNS(
        'http://www.w3.org/2000/xmlns/', 'xmlns:xlink',
        'http://www.w3.org/1999/xlink');
    this.tooltip.appendChild(svg);

    for (const areaTooltip of this.tooltips) {
      if (areaTooltip.left <= eventX && areaTooltip.top <= eventY &&
          areaTooltip.right >= eventX && areaTooltip.bottom >= eventY) {
        SVG.addText(
            svg, this.horizontalGap, this.verticalGap + this.fontSize,
            this.fontSize, areaTooltip.tooltip);
        this.showTooltipForEvent_(
            event, svg, areaTooltip.tooltipHeight, areaTooltip.tooltipWidth);
        return;
      }
    }

    if (eventX < this.bandOffsetX) {
      this.tooltip.classList.remove('active');
      return;
    }

    const eventTimestamp = this.offsetToTime(eventX - this.bandOffsetX);

    const width = 220;
    let yOffset = this.verticalGap;

    // In case of global events are not available, render tooltip for band and
    // chart.
    yOffset =
        this.updateToolTipForGlobalEvents_(event, svg, eventTimestamp, yOffset);
    if (yOffset === this.verticalGap) {
      // Find band for this mouse event.
      for (let i = 0; i < this.bands.length; ++i) {
        if (this.bands[i].top <= eventY && this.bands[i].bottom > eventY) {
          yOffset = this.updateToolTipForBand_(
              event, svg, eventTimestamp, this.bands[i].band, yOffset);
          break;
        }
      }

      // Find chart for this mouse event.
      for (let i = 0; i < this.charts.length; ++i) {
        if (this.charts[i].top <= eventY && this.charts[i].bottom > eventY) {
          yOffset = this.updateToolTipForChart_(
              event, svg, eventTimestamp, this.charts[i], yOffset);
          break;
        }
      }
    }

    if (yOffset > this.verticalGap) {
      // Content was added.
      if (this.canShowDetailedInfo()) {
        yOffset += this.lineHeight;
        SVG.addText(
            svg, this.horizontalGap, yOffset, this.fontSize,
            'Click for detailed info');
      }

      yOffset += this.verticalGap;

      this.showTooltipForEvent_(event, svg, yOffset, width);
    } else {
      this.tooltip.classList.remove('active');
    }
  }


  /**
   * Adds time information for |eventTimestamp| to the tooltip. Global time is
   * added first and VSYNC relative time is added next in case VSYNC event could
   * be found.
   *
   * @param {Object} svg tooltip container.
   * @param {number} yOffset current vertical offset.
   * @param {number} eventTimestamp timestamp of the event.
   * @returns {number} vertical position of the next element.
   */
  addTimeInfoToTooltip_(svg, yOffset, eventTimestamp) {
    const text = timestampToMsText(eventTimestamp) + ' ms';

    yOffset += this.lineHeight;
    SVG.addText(svg, this.horizontalGap, yOffset, this.fontSize, text);

    return yOffset;
  }

  /**
   * Creates and shows tooltip for global events in case they are found around
   * mouse event position.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForGlobalEvents_(event, svg, eventTimestamp, yOffset) {
    // Try to find closest global events in the range -3..3 pixels. Several
    // events may stick close each other so let diplay up to 3 closest events.
    const distanceMcs = 3 * this.resolution;
    const globalEvents = this.findGlobalEvents_(eventTimestamp, distanceMcs);
    if (globalEvents.length === 0) {
      return yOffset;
    }

    // Show the global events info.
    const globalEventCnt = Math.min(globalEvents.length, 3);
    for (let i = 0; i < globalEventCnt; ++i) {
      const globalEvent = globalEvents[i];
      const globalEventType = globalEvent[0];
      const globalEventTimestamp = globalEvent[1];

      yOffset = this.addTimeInfoToTooltip_(svg, yOffset, globalEventTimestamp);

      const attributes = eventAttributes[globalEventType];
      yOffset += this.lineHeight;
      SVG.addCircle(
          svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
          attributes.color, 'black');
      SVG.addText(
          svg, this.textOffset, yOffset, this.fontSize, attributes.name);

      // Render content if exists.
      if (globalEvent.length > 2) {
        yOffset += this.lineHeight;
        SVG.addText(
            svg, this.textOffset + this.horizontalGap, yOffset, this.fontSize,
            globalEvent[2]);
      }
    }

    yOffset += this.verticalGap;

    return yOffset;
  }

  /**
   * Creates and shows tooltip for event band for the position under |event|.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {Object} active event band.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForBand_(event, svg, eventTimestamp, eventBand, yOffset) {
    // Find the event under the cursor. |index| points to the current event
    // and |nextIndex| points to the next event.
    let nextIndex = eventBand.getFirstEvent();
    while (nextIndex >= 0) {
      if (eventBand.events[nextIndex][1] > eventTimestamp) {
        break;
      }
      nextIndex = eventBand.getNextEvent(nextIndex, 1 /* direction */);
    }
    let index = eventBand.getNextEvent(nextIndex, -1 /* direction */);

    if (index < 0 || eventBand.isEndOfSequence(index)) {
      // In case cursor points to idle event, show its interval.
      yOffset += this.lineHeight;
      const startIdle = index < 0 ? 0 : eventBand.events[index][1];
      const endIdle =
          nextIndex < 0 ? this.maxTimestamp : eventBand.events[nextIndex][1];
      SVG.addText(
          svg, this.horizontalGap, yOffset, this.fontSize,
          'Idle ' + timestampToMsText(startIdle) + '...' +
              timestampToMsText(endIdle) + ' chart time ms.');
    } else {
      // Show the sequence of non-idle events.
      // Find the start of the non-idle sequence.
      while (true) {
        const prevIndex = eventBand.getNextEvent(index, -1 /* direction */);
        if (prevIndex < 0 || eventBand.isEndOfSequence(prevIndex)) {
          break;
        }
        index = prevIndex;
      }

      const sequenceTimestamp = eventBand.events[index][1];
      yOffset = this.addTimeInfoToTooltip_(svg, yOffset, sequenceTimestamp);

      let lastTimestamp = sequenceTimestamp;
      // Scan for the entries to show.
      const entriesToShow = [];
      while (index >= 0) {
        const attributes = eventBand.getEventAttributes(index);
        const eventTimestamp = eventBand.events[index][1];
        const entryToShow = {};
        entryToShow.color = attributes.color;
        if (eventBand.events[index].length > 2) {
          entryToShow.text = attributes.name + ' ' + eventBand.events[index][2];
        } else {
          entryToShow.text = attributes.name;
        }
        if (entriesToShow.length > 0) {
          entriesToShow[entriesToShow.length - 1].text +=
              ' [' + timestampToMsText(eventTimestamp - lastTimestamp) + ' ms]';
        }
        entriesToShow.push(entryToShow);
        if (eventBand.isEndOfSequence(index)) {
          break;
        }
        lastTimestamp = eventTimestamp;
        index = eventBand.getNextEvent(index, 1 /* direction */);
      }

      // Last element is end of sequence, use bandColor for the icon.
      if (entriesToShow.length > 0) {
        entriesToShow[entriesToShow.length - 1].color = bandColor;
      }
      for (let i = 0; i < entriesToShow.length; ++i) {
        const entryToShow = entriesToShow[i];
        yOffset += this.lineHeight;
        SVG.addCircle(
            svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
            entryToShow.color, 'black');
        SVG.addText(
            svg, this.textOffset, yOffset, this.fontSize, entryToShow.text);
      }
    }

    yOffset += this.verticalGap;

    return yOffset;
  }

  /**
   * Creates and show tooltip for event chart for the position under |event|.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {Object} active event chart.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForChart_(event, svg, eventTimestamp, chart, yOffset) {
    const valueOffset = 32;

    let contentAdded = false;
    for (let i = 0; i < chart.sourcesWithBounds.length; ++i) {
      const sourceWithBounds = chart.sourcesWithBounds[i];

      let color;
      let text;
      if (sourceWithBounds.perValue) {
        // Tooltip per value
        const index = sourceWithBounds.source.getLastBefore(eventTimestamp);
        if (index < 0) {
          continue;
        }
        const event = sourceWithBounds.source.events[index];
        color = sourceWithBounds.attributes[event[2]].color;
        text = sourceWithBounds.attributes[event[2]].name;
      } else {
        // Interpolate results.
        const indexAfter =
            sourceWithBounds.source.getFirstAfter(eventTimestamp);
        if (indexAfter < 0) {
          continue;
        }
        const indexBefore = sourceWithBounds.source.getNextEvent(
            indexAfter, -1 /* direction */);
        if (indexBefore < 0) {
          continue;
        }
        const eventBefore = sourceWithBounds.source.events[indexBefore];
        const eventAfter = sourceWithBounds.source.events[indexAfter];
        let factor = (eventTimestamp - eventBefore[1]) /
            (eventAfter[1] - eventBefore[1]);

        if (!sourceWithBounds.smooth) {
          // Clamp to before value.
          if (factor < 1.0) {
            factor = 0.0;
          }
        }
        const value = factor * eventAfter[2] + (1.0 - factor) * eventBefore[2];
        text = (value * sourceWithBounds.attributes.scale).toFixed(1) + ' ' +
            sourceWithBounds.attributes.name;
        color = sourceWithBounds.attributes.color;
      }
      if (!contentAdded) {
        yOffset = this.addTimeInfoToTooltip_(svg, yOffset, eventTimestamp);
        contentAdded = true;
      }

      yOffset += this.lineHeight;
      SVG.addCircle(
          svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
          color, 'black');
      SVG.addText(svg, valueOffset, yOffset, this.fontSize, text);
    }

    if (contentAdded) {
      yOffset += this.verticalGap;
    }

    return yOffset;
  }

  /**
   * Helper that shows tooltip after filling its content.
   *
   * @param {Object} mouse event, used to determine the position of tooltip.
   * @param {Object} svg content of tooltip.
   * @param {number} height of the tooltip view.
   * @param {number} width of the tooltip view.
   */
  showTooltipForEvent_(event, svg, height, width) {
    svg.setAttribute('height', height + 'px');
    svg.setAttribute('width', width + 'px');
    this.tooltip.style.left = event.pageX + 'px';
    this.tooltip.style.top = event.pageY + 'px';
    this.tooltip.style.height = height + 'px';
    this.tooltip.style.width = width + 'px';
    this.tooltip.classList.add('active');
  }

  /**
   * Returns true in case band can show detailed info.
   */
  canShowDetailedInfo() {
    return false;
  }

  /**
   * Shows detailed info for the position under mouse event |event|. By default
   * it creates nothing.
   */
  showDetailedInfo(event) {
    return null;
  }
}

/**
 * Base class for detailed info view.
 */
class DetailedInfoView {
  discard() {}
}

/**
 * CPU detailed info view. Renders 4x zoomed CPU events split by processes and
 * threads.
 */
class CpuDetailedInfoView extends DetailedInfoView {
  create(overviewBand) {
    this.overlay = $('arc-detailed-view-overlay');
    const overviewRect = overviewBand.svg.getBoundingClientRect();

    // Clear previous content.
    this.overlay.textContent = '';

    // UI constants to render.
    const columnNameWidth = 130;
    const columnUsageWidth = 40;
    const scrollBarWidth = 3;
    const zoomFactor = 4.0;
    const cpuBandHeight = 14;
    const processInfoHeight = 14;
    const padding = 2;
    const fontSize = 12;
    const processInfoPadding = 2;
    const threadInfoPadding = 12;
    const cpuUsagePadding = 2;
    const columnsWidth = columnNameWidth + columnUsageWidth;

    // Use minimum 80% of inner width or 600 pixels to display detailed view
    // zoomed |zoomFactor| times.
    let availableWidthPixels =
        window.innerWidth * 0.8 - columnsWidth - scrollBarWidth;
    availableWidthPixels = Math.max(availableWidthPixels, 600);
    const availableForHalfBandMcs = Math.floor(
        overviewBand.offsetToTime(availableWidthPixels) / (2.0 * zoomFactor));
    // Determine the interval to display.
    const eventTimestamp =
        overviewBand.offsetToTime(event.offsetX - overviewBand.bandOffsetX);
    const minTimestamp = eventTimestamp - availableForHalfBandMcs;
    const maxTimestamp = eventTimestamp + availableForHalfBandMcs + 1;
    const duration = maxTimestamp - minTimestamp;

    // Construct sub-model of active/idle events per each thread, active within
    // this interval.
    const eventsPerTid = {};
    for (let cpuId = 0; cpuId < overviewBand.model.system.cpu.length; cpuId++) {
      const activeEvents = new Events(
          overviewBand.model.system.cpu[cpuId], 3 /* kActive */,
          3 /* kActive */);
      // Assume we have an idle before minTimestamp.
      let activeTid = 0;
      let index = activeEvents.getFirstAfter(minTimestamp);
      let activeStartTimestamp = minTimestamp;
      // Check if previous event goes over minTimestamp, in that case extract
      // the active thread.
      if (index > 0) {
        const lastBefore = activeEvents.getNextEvent(index, -1 /* direction */);
        if (lastBefore >= 0) {
          // This may be idle (tid=0) or real thread.
          activeTid = activeEvents.events[lastBefore][2];
        }
      }
      while (index >= 0 && activeEvents.events[index][1] < maxTimestamp) {
        this.addActivityTime_(
            eventsPerTid, activeTid, activeStartTimestamp,
            activeEvents.events[index][1]);
        activeTid = activeEvents.events[index][2];
        activeStartTimestamp = activeEvents.events[index][1];
        index = activeEvents.getNextEvent(index, 1 /* direction */);
      }
      this.addActivityTime_(
          eventsPerTid, activeTid, activeStartTimestamp, maxTimestamp - 1);
    }

    // The same thread might be executed on different CPU cores. Sort events.
    for (const tid in eventsPerTid) {
      eventsPerTid[tid].events.sort(function(a, b) {
        return a[1] - b[1];
      });
    }

    // Group threads by process.
    const threadsPerPid = {};
    const pids = [];
    let totalTime = 0;
    for (const tid in eventsPerTid) {
      const thread = eventsPerTid[tid];
      const pid = overviewBand.model.system.threads[tid].pid;
      if (!(pid in threadsPerPid)) {
        pids.push(pid);
        threadsPerPid[pid] = {};
        threadsPerPid[pid].totalTime = 0;
        threadsPerPid[pid].threads = [];
      }
      threadsPerPid[pid].totalTime += thread.totalTime;
      threadsPerPid[pid].threads.push(thread);
      totalTime += thread.totalTime;
    }

    // Sort processes per time usage.
    pids.sort(function(a, b) {
      return threadsPerPid[b].totalTime - threadsPerPid[a].totalTime;
    });

    const totalUsage = 100.0 * totalTime / duration;
    const cpuInfo = 'CPU view. ' + pids.length + '/' +
        Object.keys(eventsPerTid).length +
        ' active processes/threads. Total cpu usage: ' + totalUsage.toFixed(2) +
        '%.';
    const title = new EventBandTitle(
        this.overlay, undefined /* anchor */, cpuInfo, 'arc-cpu-view-title');
    const bands = new EventBands(
        title, 'arc-events-cpu-detailed-band',
        overviewBand.resolution / zoomFactor, minTimestamp, maxTimestamp);
    bands.setBandOffsetX(columnsWidth);
    const bandsWidth = bands.timestampToOffset(maxTimestamp);
    const totalWidth = bandsWidth + columnsWidth;
    bands.setWidth(totalWidth);

    for (let i = 0; i < pids.length; i++) {
      const pid = pids[i];
      const threads = threadsPerPid[pid].threads;
      let processName;
      if (pid in overviewBand.model.system.threads) {
        processName = overviewBand.model.system.threads[pid].name;
      } else {
        processName = 'Others';
      }
      const processCpuUsage = 100.0 * threadsPerPid[pid].totalTime / duration;
      const processInfo = processName + ' <' + pid + '>';
      const processInfoTextLine =
          bands.nextYOffset + processInfoHeight - padding;
      SVG.addText(
          bands.svg, processInfoPadding, processInfoTextLine, fontSize,
          processInfo);
      SVG.addText(
          bands.svg, columnsWidth - cpuUsagePadding, processInfoTextLine,
          fontSize, processCpuUsage.toFixed(2), 'end' /* anchor */);

      // Sort threads per time usage.
      threads.sort(function(a, b) {
        return eventsPerTid[b.tid].totalTime - eventsPerTid[a.tid].totalTime;
      });

      // In case we have only one main thread add CPU info to process.
      if (threads.length === 1 && threads[0].tid === pid) {
        bands.addBand(
            new Events(eventsPerTid[pid].events, 0, 1), cpuBandHeight, padding);
        bands.addBandSeparator(2 /* padding */);
        continue;
      }

      bands.nextYOffset += (processInfoHeight + padding);

      for (let j = 0; j < threads.length; j++) {
        const tid = threads[j].tid;
        bands.addBand(
            new Events(eventsPerTid[tid].events, 0, 1), cpuBandHeight, padding);
        const threadName = overviewBand.model.system.threads[tid].name;
        const threadCpuUsage = 100.0 * threads[j].totalTime / duration;
        SVG.addText(
            bands.svg, threadInfoPadding, bands.nextYOffset - padding, fontSize,
            threadName);
        SVG.addText(
            bands.svg, columnsWidth - cpuUsagePadding,
            bands.nextYOffset - 2 * padding, fontSize,
            threadCpuUsage.toFixed(2), 'end' /* anchor */);
      }
      bands.addBandSeparator(2 /* padding */);
    }

    // Add center and boundary lines.
    const kTimeMark = 10000;
    const timeEvents = [
      [kTimeMark, minTimestamp],
      [kTimeMark, eventTimestamp],
      [kTimeMark, maxTimestamp - 1],
    ];
    bands.addGlobal(new Events(timeEvents, kTimeMark, kTimeMark));

    SVG.addLine(
        bands.svg, columnNameWidth, 0, columnNameWidth, bands.height, '#888',
        0.25);

    SVG.addLine(
        bands.svg, columnsWidth, 0, columnsWidth, bands.height, '#888', 0.25);

    // Mark zoomed interval in overview.
    const overviewX = overviewBand.timestampToOffset(minTimestamp);
    const overviewWidth =
        overviewBand.timestampToOffset(maxTimestamp) - overviewX;
    this.bandSelection = SVG.addRect(
        overviewBand.svg, overviewX, 0, overviewWidth, overviewBand.height,
        '#000' /* color */, 0.1 /* opacity */);
    // Prevent band selection to capture mouse events that would lead to
    // incorrect anchor position computation for detailed view.
    this.bandSelection.classList.add('arc-no-mouse-events');

    // Align position in overview and middle line here if possible.
    const left = Math.max(
        Math.min(
            Math.round(event.clientX - columnsWidth - bandsWidth * 0.5),
            window.innerWidth - totalWidth),
        0);
    this.overlay.style.left = left + 'px';
    // Place below the overview with small gap.
    this.overlay.style.top = (overviewRect.bottom + window.scrollY + 2) + 'px';
    this.overlay.classList.add('active');
  }

  discard() {
    this.overlay.classList.remove('active');
    this.bandSelection.remove();
  }

  /**
   * Helper that adds kIdleIn/kIdleOut events into the dictionary.
   *
   * @param {Object} eventsPerTid dictionary to fill. Key is thread id and
   *     value is object that contains all events for thread with related
   *     information.
   * @param {number} tid thread id.
   * @param {number} timestampFrom start time of thread activity.
   * @param {number} timestampTo end time of thread activity.
   */
  addActivityTime_(eventsPerTid, tid, timestampFrom, timestampTo) {
    if (tid === 0) {
      // Don't process idle thread.
      return;
    }
    if (!(tid in eventsPerTid)) {
      // Create the band for the new thread.
      eventsPerTid[tid] = {};
      eventsPerTid[tid].totalTime = 0;
      eventsPerTid[tid].events = [];
      eventsPerTid[tid].tid = tid;
    }
    eventsPerTid[tid].events.push([1 /* kIdleOut */, timestampFrom]);
    eventsPerTid[tid].events.push([0 /* kIdleIn */, timestampTo]);
    // Update total time for this thread.
    eventsPerTid[tid].totalTime += (timestampTo - timestampFrom);
  }
}

class CpuEventBands extends EventBands {
  setModel(model) {
    this.model = model;
    this.showDetailedInfo = true;
    const bandHeight = 6;
    const padding = 2;
    for (let cpuId = 0; cpuId < this.model.system.cpu.length; cpuId++) {
      this.addBand(
          new Events(this.model.system.cpu[cpuId], 0, 1), bandHeight, padding);
    }
  }

  canShowDetailedInfo() {
    return this.showDetailedInfo;
  }

  showDetailedInfo(event) {
    const view = new CpuDetailedInfoView();
    view.create(this);
    return view;
  }
}

/** Represents one band with events. */
class Events {
  /**
   * Assigns events for this band. Events with type between |eventTypeMin| and
   * |eventTypeMax| are only displayed on the band.
   *
   * @param {Object[]} events non-filtered list of all events. Each has array
   *     where first element is type and second is timestamp.
   * @param {number} eventTypeMin minimum inclusive type of the event to be
   *     displayed on this band.
   * @param {number=} opt_eventTypeMax maximum inclusive type of the event to be
   *     displayed on this band. It is optional and in case is not set then
   *     range of one event type eventTypeMin is used.
   */
  constructor(events, eventTypeMin, opt_eventTypeMax) {
    this.events = events;
    this.eventTypeMin = eventTypeMin;
    if (opt_eventTypeMax) {
      this.eventTypeMax = opt_eventTypeMax;
    } else {
      this.eventTypeMax = eventTypeMin;
    }
  }

  /**
   * Helper that finds next or previous event. Events that pass filter are
   * only processed.
   *
   * @param {number} index starting index for the search, not inclusive.
   * @param {direction} direction to search, 1 means to find the next event and
   *     -1 means the previous event.
   * @returns {number} index of the next or previous event or -1 in case not
   *     found.
   */
  getNextEvent(index, direction) {
    while (true) {
      index += direction;
      if (index < 0 || index >= this.events.length) {
        return -1;
      }
      if (this.events[index][0] >= this.eventTypeMin &&
          this.events[index][0] <= this.eventTypeMax) {
        return index;
      }
    }
  }

  /**
   * Helper that finds first event. Events that pass filter are only processed.
   *
   * @returns {number} index of the first event or -1 in case not found.
   */
  getFirstEvent() {
    return this.getNextEvent(-1 /* index */, 1 /* direction */);
  }

  /**
   * Helper that returns render attributes for the event.
   *
   * @param {number} index element index in |this.events|.
   */
  getEventAttributes(index) {
    return eventAttributes[this.events[index][0]];
  }

  /**
   * Returns true if the tested event denotes end of event sequence.
   *
   * @param {number} index element index in |this.events|.
   */
  isEndOfSequence(index) {
    const nextEventTypes = endSequenceEvents[this.events[index][0]];
    if (!nextEventTypes) {
      return false;
    }
    if (nextEventTypes.length === 0) {
      return true;
    }
    const nextIndex = this.getNextEvent(index, 1 /* direction */);
    if (nextIndex < 0) {
      // No more events after and it is listed as possible end of sequence.
      return true;
    }
    return nextEventTypes.includes(this.events[nextIndex][0]);
  }

  /**
   * Returns the index of closest event to the requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getClosest(timestamp) {
    if (this.events.length === 0) {
      return -1;
    }
    if (this.events[0][1] >= timestamp) {
      return this.getFirstEvent();
    }
    if (this.events[this.events.length - 1][1] <= timestamp) {
      return this.getNextEvent(
          this.events.length /* index */, -1 /* direction */);
    }
    // At this moment |firstBefore| and |firstAfter| points to any event.
    let firstBefore = 0;
    let firstAfter = this.events.length - 1;
    while (firstBefore + 1 !== firstAfter) {
      const candidateIndex = Math.ceil((firstBefore + firstAfter) / 2);
      if (this.events[candidateIndex][1] < timestamp) {
        firstBefore = candidateIndex;
      } else {
        firstAfter = candidateIndex;
      }
    }
    // Point |firstBefore| and |firstAfter| to the supported event types.
    firstBefore =
        this.getNextEvent(firstBefore + 1 /* index */, -1 /* direction */);
    firstAfter =
        this.getNextEvent(firstAfter - 1 /* index */, 1 /* direction */);
    if (firstBefore < 0) {
      return firstAfter;
    } else if (firstAfter < 0) {
      return firstBefore;
    } else {
      const diffBefore = timestamp - this.events[firstBefore][1];
      const diffAfter = this.events[firstAfter][1] - timestamp;
      if (diffBefore < diffAfter) {
        return firstBefore;
      } else {
        return firstAfter;
      }
    }
  }

  /**
   * Returns the index of the first event after or on requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getFirstAfter(timestamp) {
    const closest = this.getClosest(timestamp);
    if (closest < 0) {
      return -1;
    }
    if (this.events[closest][1] >= timestamp) {
      return closest;
    }
    return this.getNextEvent(closest, 1 /* direction */);
  }

  /**
   * Returns the index of the last event before or on requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getLastBefore(timestamp) {
    const closest = this.getClosest(timestamp);
    if (closest < 0) {
      return -1;
    }
    if (this.events[closest][1] <= timestamp) {
      return closest;
    }
    return this.getNextEvent(closest, -1 /* direction */);
  }
}

/**
 * Helper function that creates chart with required attributes.
 *
 * @param {HTMLElement} parent container for the newly created chart.
 * @param {string} title of the chart.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {number} height of the chart in pixels.
 * @param {number} gridLinesCount number of extra intermediate grid lines, 0 i
 *                 not required.
 * @param {HTMLElement} anchor insert point. View will be added after this.
 *                             may be optional.
 *
 */
function createChart(
    parent, title, resolution, duration, height, gridLinesCount, anchor) {
  const titleBands =
      new EventBandTitle(parent, anchor, title, 'arc-events-band-title');
  const bands =
      new EventBands(titleBands, 'arc-events-band', resolution, 0, duration);
  bands.setWidth(bands.timestampToOffset(duration));
  bands.addChart(height, 4 /* padding */);
  for (let i = 0; i < gridLinesCount; i++) {
    bands.addChartGridLine((i + 1) * height / (gridLinesCount + 1));
  }
  return bands;
}
