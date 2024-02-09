/*! https://github.com/leeoniya/uPlot (v1.6.21) */
export declare class uPlot {
  /**
   * when passing a function for @targ, call init() after attaching self.root
   * to the DOM
   */
  constructor(
      opts: uPlot.Options, data?: uPlot.AlignedData,
      targ?: HTMLElement|((self: uPlot, init: Function) => void));

  /** chart container */
  readonly root: HTMLElement;

  /** status */
  readonly status: 0|1;

  /** width of the plotting area + axes in CSS pixels */
  readonly width: number;

  /**
   * height of the plotting area + axes in CSS pixels (excludes title & legend
   * height)
   */
  readonly height: number;

  /** context of canvas used for plotting area + axes */
  readonly ctx: CanvasRenderingContext2D;

  /**
   * coords of plotting area in canvas pixels (relative to full canvas w/axes)
   */
  readonly bbox: uPlot.BBox;

  /** coords of selected region in CSS pixels (relative to plotting area) */
  readonly select: uPlot.BBox;

  /** cursor state & opts*/
  readonly cursor: uPlot.Cursor;

  readonly legend: uPlot.Legend;

  //  /** focus opts */
  //  readonly focus: uPlot.Focus;

  /** series state & opts */
  readonly series: uPlot.Series[];

  /** scales state & opts */
  readonly scales: {[key: string]: uPlot.Scale;};

  /** axes state & opts */
  readonly axes: uPlot.Axis[];

  /** hooks, including any added by plugins */
  readonly hooks: uPlot.Hooks.Arrays;

  /** current data */
  readonly data: uPlot.AlignedData;

  /** .u-over dom element */
  readonly over: HTMLDivElement;

  /** .u-under dom element */
  readonly under: HTMLDivElement;

  /**
   * clears and redraws the canvas. if rebuildPaths = false, uses cached
   * series' Path2D objects
   */
  redraw(rebuildPaths?: boolean, recalcAxes?: boolean): void;

  /**
   * defers recalc & redraw for multiple ops, e.g. setScale('x', ...) &&
   * setScale('y', ...)
   */
  batch(txn: Function): void;

  /** destroys DOM, removes resize & scroll listeners, etc. */
  destroy(): void;

  /** sets the chart data & redraws. (default resetScales = true) */
  setData(data: uPlot.AlignedData, resetScales?: boolean): void;

  /** sets the limits of a scale & redraws (used for zooming) */
  setScale(scaleKey: string, limits: {min: number; max: number}): void;

  /** sets the cursor position (relative to plotting area) */
  setCursor(opts: {left: number, top: number}, fireHook?: boolean): void;

  /** sets the legend to the values of the specified idx */
  setLegend(opts: {idx: number}, fireHook?: boolean): void;

  // TODO: include other series style opts which are dynamically pulled?
  /** toggles series visibility or focus */
  setSeries(
      seriesIdx: number|null, opts: {show?: boolean, focus?: boolean},
      fireHook?: boolean): void;

  /** adds a series */
  addSeries(opts: uPlot.Series, seriesIdx?: number): void;

  /** deletes a series */
  delSeries(seriesIdx: number): void;

  /** adds a band */
  addBand(opts: uPlot.Band, bandIdx?: number): void;

  /** modifies an existing band */
  setBand(bandIdx: number, opts: uPlot.Band): void;

  /** deletes a band. if null bandIdx, clears all bands */
  delBand(bandIdx?: number|null): void;

  /**
   * sets visually selected region without triggering setScale (zoom). (default
   * fireHook = true)
   */
  setSelect(
      opts: {left: number, top: number, width: number, height: number},
      fireHook?: boolean): void;

  /**
   * sets the width & height of the plotting area + axes (excludes title &
   * legend height)
   */
  setSize(opts: {width: number; height: number}): void;

  /**
   * converts a CSS pixel position (relative to plotting area) to the closest
   * data index
   */
  posToIdx(left: number, canvasPixels?: boolean): number;

  /**
   * converts a CSS pixel position (relative to plotting area) to a value along
   * the given scale
   */
  posToVal(leftTop: number, scaleKey: string, canvasPixels?: boolean): number;

  /**
   * converts a value along the given scale to a CSS (default) or canvas pixel
   * position. (default canvasPixels = false)
   */
  valToPos(val: number, scaleKey: string, canvasPixels?: boolean): number;

  /** converts a value along x to the closest data index */
  valToIdx(val: number): number;

  /**
   * updates getBoundingClientRect() cache for cursor positioning. use when
   * plot's position changes (excluding window scroll & resize)
   */
  syncRect(defer?: boolean): void;

  /** uPlot's path-builder factories */
  static paths: uPlot.Series.PathBuilderFactories;

  /** a deep merge util fn */
  static assign(targ: object, ...srcs: object[]): object;

  /**
   * re-ranges a given min/max by a multiple of the range's magnitude (used
   * internally to expand/snap/pad numeric y scales)
   */
  static rangeNum(min: number, max: number, mult: number, extra: boolean):
      uPlot.Range.MinMax;
  static rangeNum(min: number, max: number, cfg: uPlot.Range.Config):
      uPlot.Range.MinMax;

  /**
   * re-ranges a given min/max outwards to nearest 10% of given min/max's
   * magnitudes, unless fullMags = true
   */
  static rangeLog(
      min: number, max: number, base: uPlot.Scale.LogBase,
      fullMags: boolean): uPlot.Range.MinMax;

  /**
   * re-ranges a given min/max outwards to nearest 10% of given min/max's
   * magnitudes, unless fullMags = true
   */
  static rangeAsinh(
      min: number, max: number, base: uPlot.Scale.LogBase,
      fullMags: boolean): uPlot.Range.MinMax;

  /**
   * default numeric formatter using browser's locale: new
   * Intl.NumberFormat(navigator.language).format
   */
  static fmtNum(val: number): string;

  /**
   * creates an efficient formatter for Date objects from a template string,
   * e.g. {YYYY}-{MM}-{DD}
   */
  static fmtDate(tpl: string, names?: uPlot.DateNames): (date: Date) => string;

  /**
   * converts a Date into new Date that's time-adjusted for the given IANA Time
   * Zone Name
   */
  static tzDate(date: Date, tzName: string): Date;

  /** outerJoins multiple data tables on table[0] values */
  static join(tables: uPlot.AlignedData[], nullModes?: uPlot.JoinNullMode[][]):
      uPlot.AlignedData;

  static addGap: uPlot.Series.AddGap;

  static clipGaps: uPlot.Series.ClipPathBuilder;

  /**
   * helper function for grabbing proper drawing orientation vars and fns for a
   * plot instance (all dims in canvas pixels)
   */
  static orient(u: uPlot, seriesIdx: number, callback: uPlot.OrientCallback):
      any;

  /** returns a pub/sub instance shared by all plots usng the provided key */
  static sync(key: string): uPlot.SyncPubSub;

  /**
   * cached devicePixelRatio (faster than reading it from
   * window.devicePixelRatio)
   */
  static pxRatio: number;
}

export declare namespace uPlot {
  type OrientCallback = (
      series: Series,
      dataX: number[],
      dataY: (number|null)[],
      scaleX: Scale,
      scaleY: Scale,
      valToPosX: ValToPos,
      valToPosY: ValToPos,
      xOff: number,
      yOff: number,
      xDim: number,
      yDim: number,
      moveTo: MoveToH|MoveToV,
      lineTo: LineToH|LineToV,
      rect: RectH|RectV,
      arc: ArcH|ArcV,
      bezierCurveTo: BezierCurveToH|BezierCurveToV,
      ) => any;

  type ValToPos =
      (val: number, scale: Scale, fullDim: number, offset: number) => number;

  type Drawable = Path2D|CanvasRenderingContext2D;

  type MoveToH = (p: Drawable, x: number, y: number) => void;
  type MoveToV = (p: Drawable, y: number, x: number) => void;
  type LineToH = (p: Drawable, x: number, y: number) => void;
  type LineToV = (p: Drawable, y: number, x: number) => void;
  type RectH = (p: Drawable, x: number, y: number, w: number, h: number) =>
      void;
  type RectV = (p: Drawable, y: number, x: number, h: number, w: number) =>
      void;
  type ArcH =
      (p: Drawable, x: number, y: number, r: number, startAngle: number,
       endAngle: number) => void;
  type ArcV =
      (p: Drawable, y: number, x: number, r: number, startAngle: number,
       endAngle: number) => void;
  type BezierCurveToH =
      (p: Drawable, bp1x: number, bp1y: number, bp2x: number, bp2y: number,
       p2x: number, p2y: number) => void;
  type BezierCurveToV =
      (p: Drawable, bp1y: number, bp1x: number, bp2y: number, bp2x: number,
       p2y: number, p2x: number) => void;

  export const enum JoinNullMode {
    /** use for series with spanGaps: true */
    Remove = 0,
    /** retain explicit nulls gaps (default) */
    Retain = 1,
    /**
       expand explicit null gaps to include adjacent alignment artifacts
       (undefined values)
     */
    Expand = 2,
  }

  export const enum Orientation {
    Horizontal = 0,
    Vertical = 1,
  }

  export type TypedArray = Int8Array|Uint8Array|Int16Array|Uint16Array|
      Int32Array|Uint32Array|Uint8ClampedArray|Float32Array|Float64Array;

  export type AlignedData =
      [
        xValues: number[]|TypedArray,
        ...yValues: ((number|null|undefined)[]|TypedArray)[],
      ]

      export interface DateNames {
    /** long month names */
    MMMM: string[];

    /** short month names */
    MMM: string[];

    /** long weekday names (0: Sunday) */
    WWWW: string[];

    /** short weekday names (0: Sun) */
    WWW: string[];
  }

  export namespace Range {
    export type MinMax = [min: number|null, max: number|null];

    export type Function =
        (self: uPlot, initMin: number, initMax: number,
         scaleKey: string) => MinMax;

    export type SoftMode = 0|1|2|3;

    export interface Limit {
      /** initial multiplier for dataMax-dataMin delta */
      pad?: number;  // 0.1

      /** soft limit */
      soft?: number;  // 0

      /**
       * soft limit active if... 0: never, 1: data <= limit, 2: data + padding
       * <= limit, 3: data <= limit <= data + padding
       */
      mode?: SoftMode;  // 3

      /** hard limit */
      hard?: number;
    }

    export interface Config {
      min: Range.Limit;
      max: Range.Limit;
    }
  }

  export interface Scales {
    [key: string]: Scale;
  }

  type SidesWithAxes =
      [top: boolean, right: boolean, bottom: boolean, left: boolean];

  export type PaddingSide = number|null|
      ((self: uPlot, side: Axis.Side, sidesWithAxes: SidesWithAxes,
        cycleNum: number) => number);

  export type Padding = [
    top: PaddingSide, right: PaddingSide, bottom: PaddingSide, left: PaddingSide
  ];

  export interface Legend {
    show?: boolean;  // true
    /** show series values at current cursor.idx */
    live?: boolean;  // true
    /** swiches primary interaction mode to toggle-one/toggle-all */
    isolate?: boolean;  // false
    /** series indicators */
    markers?: Legend.Markers;

    /** current index (readback-only, not for init) */
    idx?: number|null;
    /** current indices (readback-only, not for init) */
    idxs?: (number|null)[];
    /** current values (readback-only, not for init) */
    values?: Legend.Values;
  }

  export namespace Legend {
    export type Width = number|((self: uPlot, seriesIdx: number) => number);

    export type Stroke = CSSStyleDeclaration['borderColor']|
        ((self: uPlot,
          seriesIdx: number) => CSSStyleDeclaration['borderColor']);

    export type Dash = CSSStyleDeclaration['borderStyle']|
        ((self: uPlot,
          seriesIdx: number) => CSSStyleDeclaration['borderStyle']);

    export type Fill = CSSStyleDeclaration['background']|
        ((self: uPlot, seriesIdx: number) => CSSStyleDeclaration['background']);

    export type Value = {[key: string]: string|number;};

    export type Values = Value[];

    export interface Markers {
      show?: boolean;  // true
      /** series indicator line width */
      width?: Legend.Width;
      /** series indicator stroke (CSS borderColor) */
      stroke?: Legend.Stroke;
      /** series indicator fill */
      fill?: Legend.Fill;
      /** series indicator stroke style (CSS borderStyle) */
      dash?: Legend.Dash;
    }
  }

  export type DateFormatterFactory = (tpl: string) => (date: Date) => string;

  export type LocalDateFromUnix = (ts: number) => Date;

  export const enum DrawOrderKey {
    Axes = 'axes',
    Series = 'series',
  }

  export const enum Mode {
    Aligned = 1,
    Faceted = 2,
  }

  export interface Options {
    /**
     * 1: aligned & ordered, single-x / y-per-series, 2: unordered & faceted,
     * per-series/per-point x,y,size,label,color,shape,etc.
     */
    mode?: Mode,

        /** chart title */
        title?: string;

    /** id to set on chart div */
    id?: string;

    /** className to add to chart div */
    class
    ?: string;

    /** width of plotting area + axes in CSS pixels */
    width: number;

    /**
     * height of plotting area + axes in CSS pixels (excludes title & legend
     * height)
     */
    height: number;

    /** data for chart, if none is provided as argument to constructor */
    data?: AlignedData;

    /**
     * converts a unix timestamp to Date that's time-adjusted for the desired
     * timezone
     */
    tzDate?: LocalDateFromUnix;

    /**
     * creates an efficient formatter for Date objects from a template string,
     * e.g. {YYYY}-{MM}-{DD}
     */
    fmtDate?: DateFormatterFactory;

    /** timestamp multiplier that yields 1 millisecond */
    ms?: 1e-3|1;  // 1e-3

    /** drawing order for axes/grid & series (default: ["axes", "series"]) */
    drawOrder?: DrawOrderKey[];

    /**
     * whether vt & hz lines of series/grid/ticks should be crisp/sharp or
     * sub-px antialiased
     */
    pxAlign?: boolean|number;  // true

    series: Series[];

    bands?: Band[];

    scales?: Scales;

    axes?: Axis[];

    /**
     * padding per side, in CSS pixels (can prevent cross-axis labels at the
     * plotting area limits from being chopped off)
     */
    padding?: Padding;

    select?: Select;

    legend?: Legend;

    cursor?: Cursor;

    focus?: Focus;

    hooks?: Hooks.Arrays;

    plugins?: Plugin[];
  }

  export interface Focus {
    /** alpha-transparancy of de-focused series */
    alpha: number;
  }

  export interface BBox {
    show?: boolean;
    left: number;
    top: number;
    width: number;
    height: number;
  }

  export interface Select extends BBox {
    /** div into which .u-select will be placed: .u-over or .u-under */
    over?: boolean;  // true
  }

  export interface SyncPubSub {
    key: string;
    sub: (client: uPlot) => void;
    unsub: (client: uPlot) => void;
    pub:
        (type: string, client: uPlot, x: number, y: number, w: number,
         h: number, i: number) => void;
    plots: uPlot[];
  }

  export namespace Cursor {
    export type LeftTop = [left: number, top: number];

    export type MouseListener = (e: MouseEvent) => null;

    export type MouseListenerFactory =
        (self: uPlot, targ: HTMLElement,
         handler: MouseListener) => MouseListener|null;

    export type DataIdxRefiner =
        (self: uPlot, seriesIdx: number, closestIdx: number,
         xValue: number) => number;

    export type MousePosRefiner =
        (self: uPlot, mouseLeft: number, mouseTop: number) => LeftTop;

    export interface Bind {
      mousedown?: MouseListenerFactory;
      mouseup?: MouseListenerFactory;
      click?: MouseListenerFactory;
      dblclick?: MouseListenerFactory;

      mousemove?: MouseListenerFactory;
      mouseleave?: MouseListenerFactory;
      mouseenter?: MouseListenerFactory;
    }

    export namespace Points {
      export type Show = boolean|
          ((self: uPlot, seriesIdx: number) => HTMLElement);
      export type Size = number|((self: uPlot, seriesIdx: number) => number);
      export type BBox = (self: uPlot, seriesIdx: number) => uPlot.BBox;
      export type Width = number|
          ((self: uPlot, seriesIdx: number, size: number) => number);
      export type Stroke = CanvasRenderingContext2D['strokeStyle']|
          ((self: uPlot,
            seriesIdx: number) => CanvasRenderingContext2D['strokeStyle']);
      export type Fill = CanvasRenderingContext2D['fillStyle']|
          ((self: uPlot,
            seriesIdx: number) => CanvasRenderingContext2D['fillStyle']);
    }

    export interface Points {
      show?: Points.Show;
      /** hover point diameter in CSS pixels */
      size?: Points.Size;
      /** hover point bbox in CSS pixels (will be used instead of size) */
      bbox?: Points.BBox;
      /** hover point outline width in CSS pixels */
      width?: Points.Width;
      /** hover point outline color, pattern or gradient */
      stroke?: Points.Stroke;
      /** hover point fill color, pattern or gradient */
      fill?: Points.Fill;
    }

    export interface Drag {
      setScale?: boolean;  // true
      /** toggles dragging along x */
      x?: boolean;  // true
      /** toggles dragging along y */
      y?: boolean;  // false
      /** min drag distance threshold */
      dist?: number;  // 0
      /**
       * when x & y are true, sets an upper drag limit in CSS px for
       * adaptive/unidirectional behavior
       */
      uni?: number;  // null
    }

    export namespace Sync {
      export type Scales = [xScaleKey: string|null, yScaleKey: string|null];

      export type Filter =
          (type: string, client: uPlot, x: number, y: number, w: number,
           h: number, i: number) => boolean;

      export interface Filters {
        /** filters emitted events */
        pub?: Filter;
        /** filters received events */
        sub?: Filter;
      }

      export type ScaleKeyMatcher =
          (subScaleKey: string|null, pubScaleKey: string|null) => boolean;

      export type Match = [matchX: ScaleKeyMatcher, matchY: ScaleKeyMatcher];

      export type Values = [xScaleValue: number, yScaleValue: number];
    }

    export interface Sync {
      /** sync key must match between all charts in a synced group */
      key: string;
      /**
       * determines if series toggling and focus via cursor is synced across
       * charts
       */
      setSeries?: boolean;  // true
      /**
       * sets the x and y scales to sync by values. null will sync by relative
       * (%) position
       */
      scales?: Sync.Scales;  // [xScaleKey, null]
      /** fns that match x and y scale keys between publisher and subscriber */
      match?: Sync.Match;
      /** event filters */
      filters?: Sync.Filters;
      /**
       * sync scales' values at the cursor position (exposed for read-back by
       * subscribers)
       */
      values?: Sync.Values,
    }

    export interface Focus {
      /**
       * minimum cursor proximity to datapoint in CSS pixels for focus
       * activation
       */
      prox: number;
    }
  }

  export interface Cursor {
    /** cursor on/off */
    show?: boolean;

    /** vertical crosshair on/off */
    x?: boolean;

    /** horizontal crosshair on/off */
    y?: boolean;

    /** cursor position left offset in CSS pixels (relative to plotting area) */
    left?: number;

    /** cursor position top offset in CSS pixels (relative to plotting area) */
    top?: number;

    /** closest data index to cursor (closestIdx) */
    idx?: number|null;

    /**
     * returns data idx used for hover points & legend display (defaults to
     * closestIdx)
     */
    dataIdx?: Cursor.DataIdxRefiner;

    /** a series-matched array of indices returned by dataIdx() */
    idxs?: (number|null)[];

    /**
     * fires on debounced mousemove events; returns refined [left, top] tuple
     * to snap cursor position
     */
    move?: Cursor.MousePosRefiner;

    /** series hover points */
    points?: Cursor.Points;

    /**
     * event listener proxies (can be overridden to tweak interaction behavior)
     */
    bind?: Cursor.Bind;

    /** determines vt/hz cursor dragging to set selection & setScale (zoom) */
    drag?: Cursor.Drag;

    /** sync cursor between multiple charts */
    sync?: Cursor.Sync;

    /** focus series closest to cursor */
    focus?: Cursor.Focus;

    /** lock cursor on mouse click in plotting area */
    lock?: boolean;  // false
  }

  export namespace Scale {
    export type Auto = boolean|((self: uPlot, resetScales: boolean) => boolean);

    export type Range = Range.MinMax|Range.Function|Range.Config;

    export const enum Distr {
      Linear = 1,
      Ordinal = 2,
      Logarithmic = 3,
      ArcSinh = 4,
    }

    export type LogBase = 10|2;

    export type Clamp = number|
        ((self: uPlot, val: number, scaleMin: number, scaleMax: number,
          scaleKey: string) => number);
  }

  export interface Scale {
    /** is this scale temporal, with series' data in UNIX timestamps? */
    time?: boolean;

    /**
     * determines whether all series' data on this scale will be scanned to
     * find the full min/max range
     */
    auto?: Scale.Auto;

    /**
     * can define a static scale range or re-range an initially-determined
     * range from series data
     */
    range?: Scale.Range;

    /** scale key from which this scale is derived */
    from?: string;

    /** scale distribution. 1: linear, 2: ordinal, 3: logarithmic, 4: arcsinh */
    distr?: Scale.Distr;  // 1

    /** logarithmic base */
    log?: Scale.LogBase;  // 10;

    /** clamps log scale values <= 0 (default = scaleMin / 10) */
    clamp?: Scale.Clamp;

    /** arcsinh linear threshold */
    asinh?: number;  // 1

    /** current min scale value */
    min?: number;

    /** current max scale value */
    max?: number;

    /** scale direction */
    dir?: 1|- 1;

    /** scale orientation - 0: hz, 1: vt */
    ori?: 0|1;

    /** own key (for read-back) */
    key?: string;
  }

  export namespace Series {
    export interface Paths {
      /** path to stroke */
      stroke?: Path2D|Map<CanvasRenderingContext2D['strokeStyle'], Path2D>|null;

      /** path to fill */
      fill?: Path2D|Map<CanvasRenderingContext2D['fillStyle'], Path2D>|null;

      /** path for clipping fill & stroke (used for gaps) */
      clip?: Path2D|null;

      /**
       * yMin-ward (dir: -1) and/or yMax-ward (dir: 1) clips built using the
       * stroke path (inverted dirs from band.dir fills)
       */
      band?: Path2D|null|[yMinClip: Path2D, yMaxClip: Path2D];

      /**
       * tuples of canvas pixel coordinates that were used to construct the
       * gaps clip
       */
      gaps?: [from: number, to: number][];

      /**
       * bitmap of whether the band clip should be applied to stroke, fill, or
       * both
       */
      flags?: number;
    }

    export interface SteppedPathBuilderOpts {
      align?: -1|1;  // 1

      alignGaps?: -1|0|1;  // 0

      // whether to draw ascenders/descenders at null/gap boundaries
      ascDesc?: boolean;  // false
    }

    export const enum BarsPathBuilderFacetUnit {
      ScaleValue = 1,
      PixelPercent = 2,
      Color = 3,
    }

    export const enum BarsPathBuilderFacetKind {
      Unary = 1,
      Discrete = 2,
      Continuous = 3,
    }

    export type BarsPathBuilderFacetValue =
        string|number|boolean|null|undefined;

    export interface BarsPathBuilderFacet {
      /** unit of measure for output of values() */
      unit: BarsPathBuilderFacetUnit;
      /** are the values unary, discrete, or continuous */
      kind?: BarsPathBuilderFacetKind;
      /** values to use for this facet */
      values:
          (self: uPlot, seriesIdx: number, idx0: number,
           idx1: number) => BarsPathBuilderFacetValue[];
    }

    /** custom per-datapoint styling and positioning */
    export interface BarsPathBuilderDisplay {
      x0?: BarsPathBuilderFacet;
      //  x1?: BarsPathBuilderFacet;
      y0?: BarsPathBuilderFacet;
      y1?: BarsPathBuilderFacet;
      size?: BarsPathBuilderFacet;
      fill?: BarsPathBuilderFacet;
      stroke?: BarsPathBuilderFacet;
    }

    export interface BarsPathBuilderOpts {
      align?: -1|0|1;  // 0

      size?: [factor?: number, max?: number, min?: number];

      // corner radius factor of bar size (0 - 0.5)
      radius?: number;  // 0

      /** fixed-size gap between bars in CSS pixels (reduces bar width) */
      gap?: number;

      /**
       * should return a custom [cached] layout for bars in % of plotting area
       * (0..1)
       */
      disp?: BarsPathBuilderDisplay;

      /**
       * called with bbox geometry of each drawn bar in canvas pixels. useful
       * for spatial index, etc.
       */
      each?:
          (self: uPlot, seriesIdx: number, idx: number, left: number,
           top: number, width: number, height: number) => void;
    }

    export interface LinearPathBuilderOpts {
      alignGaps?: -1|0|1;  // 0
    }

    export interface SplinePathBuilderOpts {
      alignGaps?: -1|0|1;  // 0
    }

    export type PointsPathBuilderFactory = () => Points.PathBuilder;
    export type LinearPathBuilderFactory = (opts?: LinearPathBuilderOpts) =>
        Series.PathBuilder;
    export type SplinePathBuilderFactory = (opts?: SplinePathBuilderOpts) =>
        Series.PathBuilder;
    export type SteppedPathBuilderFactory = (opts?: SteppedPathBuilderOpts) =>
        Series.PathBuilder;
    export type BarsPathBuilderFactory = (opts?: BarsPathBuilderOpts) =>
        Series.PathBuilder;

    export interface PathBuilderFactories {
      linear?: LinearPathBuilderFactory;
      spline?: SplinePathBuilderFactory;
      stepped?: SteppedPathBuilderFactory;
      bars?: BarsPathBuilderFactory;
      points?: PointsPathBuilderFactory;
    }

    export type Stroke = CanvasRenderingContext2D['strokeStyle']|
        ((self: uPlot,
          seriesIdx: number) => CanvasRenderingContext2D['strokeStyle']);

    export type Fill = CanvasRenderingContext2D['fillStyle']|
        ((self: uPlot,
          seriesIdx: number) => CanvasRenderingContext2D['fillStyle']);

    export type Cap = CanvasRenderingContext2D['lineCap'];

    export namespace Points {
      export interface Paths {
        /** path to stroke */
        stroke?: Path2D|null;

        /** path to fill */
        fill?: Path2D|null;

        /** path for clipping fill & stroke */
        clip?: Path2D|null;

        /**
         * bitmap of whether the clip should be applied to stroke, fill, or
         * both
         */
        flags?: number;
      }

      export type Show = boolean|
          ((self: uPlot, seriesIdx: number, idx0: number,
            idx1: number) => boolean|undefined);

      export type Filter = number[]|null|
          ((self: uPlot, seriesIdx: number, show: boolean,
            gaps?: null|number[][]) => number[]|null);

      export type PathBuilder =
          (self: uPlot, seriesIdx: number, idx0: number, idx1: number,
           filtIdxs?: number[]|null) => Paths|null;
    }

    export interface Points {
      /**
       * if boolean or returns boolean, round points are drawn with defined
       * options, else fn should draw own custom points via self.ctx
       */
      show?: Points.Show;

      paths?: Points.PathBuilder;

      /** may return an array of points indices to draw */
      filter?: Points.Filter;

      /** diameter of point in CSS pixels */
      size?: number;

      /**
       * minimum avg space between point centers before they're shown (default:
       * size * 2)
       */
      space?: number;

      /** line width of circle outline in CSS pixels */
      width?: number;

      /** line color of circle outline (defaults to series.stroke) */
      stroke?: Stroke;

      /** line dash segment array */
      dash?: number[];

      /** line cap */
      cap?: Series.Cap;

      /** fill color of circle (defaults to #fff) */
      fill?: Fill;
    }

    export interface Facet {
      scale: string;

      auto?: boolean;

      sorted?: Sorted;
    }

    export type Gap = [from: number, to: number];

    export type Gaps = Gap[];

    export type GapsRefiner = Gaps|
        ((self: uPlot, seriesIdx: number, idx0: number, idx1: number,
          nullGaps: Gaps) => Gaps);

    export type AddGap = (gaps: Gaps, from: number, to: number) => void;

    export type ClipPathBuilder =
        (gaps: Gaps, ori: Orientation, left: number, top: number, width: number,
         height: number) => Path2D|null;

    export type PathBuilder =
        (self: uPlot, seriesIdx: number, idx0: number,
         idx1: number) => Paths|null;

    export type MinMaxIdxs = [minIdx: number, maxIdx: number];

    export type Value = string|
        ((self: uPlot, rawValue: number, seriesIdx: number,
          idx: number) => string|number);

    export type Values = (self: uPlot, seriesIdx: number, idx: number) =>
        object;

    export type FillTo = number|
        ((self: uPlot, seriesIdx: number, dataMin: number,
          dataMax: number) => number);

    export const enum Sorted {
      Unsorted = 0,
      Ascending = 1,
      Descending = -1,
    }
  }

  export interface Series {
    /** series on/off. when off, it will not affect its scale */
    show?: boolean;

    /** className to add to legend parts and cursor hover points */
    class
    ?: string;

    /** scale key */
    scale?: string;

    /** whether this series' data is scanned during auto-ranging of its scale */
    auto?: boolean;  // true

    /** if & how the data is pre-sorted (scale.auto optimization) */
    sorted?: Series.Sorted;

    /** when true, null data values will not cause line breaks */
    spanGaps?: boolean;

    /** may mutate and/or augment gaps array found from null values */
    gaps?: Series.GapsRefiner;

    /**
     * whether path and point drawing should offset canvas to try drawing crisp
     * lines
     */
    pxAlign?: number|boolean;  // 1

    /** legend label */
    label?: string;

    /**
     * inline-legend value formatter. can be an fmtDate formatting string when
     * scale.time: true
     */
    value?: Series.Value;

    /** table-legend multi-values formatter */
    values?: Series.Values;

    paths?: Series.PathBuilder;

    /** rendered datapoints */
    points?: Series.Points;

    /** facets */
    facets?: Series.Facet[];

    /** line width in CSS pixels */
    width?: number;

    /** line & legend color */
    stroke?: Series.Stroke;

    /** area fill & legend color */
    fill?: Series.Fill;

    /** area fill baseline (default: 0) */
    fillTo?: Series.FillTo;

    /** line dash segment array */
    dash?: number[];

    /** line cap */
    cap?: Series.Cap;

    /** alpha-transparancy */
    alpha?: number;

    /** current min and max data indices rendered */
    idxs?: Series.MinMaxIdxs;

    /** current min rendered value */
    min?: number;

    /** current max rendered value */
    max?: number;
  }

  export namespace Band {
    export type Fill = CanvasRenderingContext2D['fillStyle']|
        ((self: uPlot, bandIdx: number,
          highSeriesFill: CanvasRenderingContext2D['fillStyle']) =>
             CanvasRenderingContext2D['fillStyle']);

    export type Bounds = [fromSeriesIdx: number, toSeriesIdx: number];
  }

  export interface Band {
    /** band on/off */
    //  show?: boolean;

    /** series indices of upper and lower band edges */
    series: Band.Bounds;

    /** area fill style */
    fill?: Band.Fill;

    /**
     * whether to fill towards yMin (-1) or yMax (+1) between "from" & "to"
     * series
     */
    dir?: 1|- 1;  // -1
  }

  export namespace Axis {
    /** must return an array of same length as splits, e.g. via splits.map() */
    export type Filter =
        (self: uPlot, splits: number[], axisIdx: number, foundSpace: number,
         foundIncr: number) => (number|null)[];

    export type Size = number|
        ((self: uPlot, values: string[], axisIdx: number,
          cycleNum: number) => number);

    export type Space = number|
        ((self: uPlot, axisIdx: number, scaleMin: number, scaleMax: number,
          plotDim: number) => number);

    export type Incrs = number[]|
        ((self: uPlot, axisIdx: number, scaleMin: number, scaleMax: number,
          fullDim: number, minSpace: number) => number[]);

    export type Splits = number[]|
        ((self: uPlot, axisIdx: number, scaleMin: number, scaleMax: number,
          foundIncr: number, foundSpace: number) => number[]);

    export type StaticValues = (string|number|null)[];

    export type DynamicValues =
        (self: uPlot, splits: number[], axisIdx: number, foundSpace: number,
         foundIncr: number) => StaticValues;

    export type TimeValuesConfig = (string|number|null)[][];

    export type TimeValuesTpl = string;

    export type Values =
        StaticValues|DynamicValues|TimeValuesTpl|TimeValuesConfig;

    export type Stroke = CanvasRenderingContext2D['strokeStyle']|
        ((self: uPlot,
          axisIdx: number) => CanvasRenderingContext2D['strokeStyle']);

    export const enum Side {
      Top = 0,
      Right = 1,
      Bottom = 2,
      Left = 3,
    }

    export const enum Align {
      Left = 1,
      Right = 2,
    }

    export type Rotate = number|
        ((self: uPlot, values: (string|number)[], axisIdx: number,
          foundSpace: number) => number);

    interface OrthoLines {
      /** on/off */
      show?: boolean;  // true

      /** line color */
      stroke?: Stroke;

      /** line width in CSS pixels */
      width?: number;

      /** line dash segment array */
      dash?: number[];

      /** line cap */
      cap?: Series.Cap;
    }

    export interface Border extends OrthoLines {}

    interface FilterableOrthoLines extends OrthoLines {
      /**
       * can filter which splits render lines. e.g splits.map(v => v % 2 === 0 ?
       * v : null)
       */
      filter?: Filter;
    }

    export interface Grid extends FilterableOrthoLines {}

    export interface Ticks extends FilterableOrthoLines {
      /** length of tick in CSS pixels */
      size?: number;
    }
  }

  export interface Axis {
    /** axis on/off */
    show?: boolean;

    /** scale key */
    scale?: string;

    /** side of chart - 0: top, 1: rgt, 2: btm, 3: lft */
    side?: Axis.Side;

    /**
     * height of x axis or width of y axis in CSS pixels alloted for values,
     * gap & ticks, but excluding axis label
     */
    size?: Axis.Size;

    /**
     * gap between axis values and axis baseline (or ticks, if enabled) in CSS
     * pixels
     */
    gap?: number;

    /** font used for axis values */
    font?: CanvasRenderingContext2D['font'];

    /** color of axis label & values */
    stroke?: Axis.Stroke;

    /** axis label text */
    label?: string;

    /**
     * height of x axis label or width of y axis label in CSS pixels alloted
     * for label text + labelGap
     */
    labelSize?: number;

    /** gap between label baseline and tick values in CSS pixels */
    labelGap?: number;

    /** font used for axis label */
    labelFont?: CanvasRenderingContext2D['font'];

    /** minimum grid & tick spacing in CSS pixels */
    space?: Axis.Space;

    /** available divisors for axis ticks, values, grid */
    incrs?: Axis.Incrs;

    /**
     * determines how and where the axis must be split for placing ticks,
     * values, grid
     */
    splits?: Axis.Splits;

    /**
     * can filter which splits are passed to axis.values() for rendering. e.g
     * splits.map(v => v % 2 === 0 ? v : null)
     */
    filter?: Axis.Filter;

    /** formats values for rendering */
    values?: Axis.Values;

    /**
     * values rotation in degrees off horizontal (only bottom axes w/ side: 2)
     */
    rotate?: Axis.Rotate;

    /** text alignment of axis values - 1: left, 2: right */
    align?: Axis.Align;

    /** gridlines to draw from this axis' splits */
    grid?: Axis.Grid;

    /** ticks to draw from this axis' splits */
    ticks?: Axis.Ticks;

    /** axis border/edge rendering */
    border?: Axis.Border;
  }

  export namespace Hooks {
    export interface Defs {
      /**
       * fires after opts are defaulted & merged but data has not been set and
       * scales have not been ranged
       */
      init?: (self: uPlot, opts: Options, data: AlignedData) => void;

      /**
       * fires after each initial and subsequent series addition (discern via
       * self.status === 0 or 1)
       */
      addSeries?: (self: uPlot, seriesIdx: number) => void;

      /** fires after each series deletion */
      delSeries?: (self: uPlot, seriesIdx: number) => void;

      /** fires after any scale has changed */
      setScale?: (self: uPlot, scaleKey: string) => void;

      /** fires after the cursor is moved */
      setCursor?: (self: uPlot) => void;

      /** fires when cursor changes idx and legend updates (or should update) */
      setLegend?: (self: uPlot) => void;

      /** fires after a selection is completed */
      setSelect?: (self: uPlot) => void;

      /** fires after a series is toggled or focused */
      setSeries?: (self: uPlot, seriesIdx: number|null, opts: Series) => void;

      /** fires after data is updated updated */
      setData?: (self: uPlot) => void;

      /** fires after the chart is resized */
      setSize?: (self: uPlot) => void;

      /** fires at start of every redraw */
      drawClear?: (self: uPlot) => void;

      /** fires after all axes are drawn */
      drawAxes?: (self: uPlot) => void;

      /** fires after each series is drawn */
      drawSeries?: (self: uPlot, seriesIdx: number) => void;

      /** fires after everything is drawn */
      draw?: (self: uPlot) => void;

      /** fires after the chart is fully initialized and in the DOM */
      ready?: (self: uPlot) => void;

      /** fires after the chart is destroyed */
      destroy?: (self: uPlot) => void;

      /**
       * fires after .u-over's getBoundingClientRect() is called (due to scroll
       * or resize events)
       */
      syncRect?: (self: uPlot, rect: DOMRect) => void;
    }

    export type Arrays =
    {[P in keyof Defs]: Defs[P][]}

    export type ArraysOrFuncs =
    {[P in keyof Defs]: Defs[P][]|Defs[P]}
  }

  export interface Plugin {
    /** can mutate provided opts as necessary */
    opts?: (self: uPlot, opts: Options) => void | Options;
    hooks: Hooks.ArraysOrFuncs;
  }
}
