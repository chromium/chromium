// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_OP_HELPER_H_
#define CC_TEST_PAINT_OP_HELPER_H_

#include <sstream>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/skottie_wrapper.h"

namespace cc {

// A helper class to help with debugging PaintOp/PaintOpBuffer.
// Note that this file is primarily used for debugging. As such, it isn't
// typically a part of BUILD.gn (except for self-testing), so all of the
// implementation should be limited ot the header.
class PaintOpHelper {
 public:
  // Print optional/nullable wrapper types are using underlying types.
  template <typename T>
  static std::string ToString(const T* ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(const sk_sp<T>& ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(scoped_refptr<T> ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(const std::optional<T>& opt) {
    return opt.has_value() ? ToString(*opt) : "(nil)";
  }

  template <typename T>
  static std::string ToString(const std::vector<T>& vec) {
    std::ostringstream str;
    str << "{";
    bool is_first = true;
    for (const T& element : vec) {
      if (!is_first) {
        str << ", ";
      }
      str << ToString(element);
      is_first = false;
    }
    str << "}";
    return str.str();
  }

  static std::string ToString(const PaintOp& base_op) {
    std::ostringstream str;
    str << std::boolalpha << base_op.GetType() << "(";
    switch (base_op.GetType()) {
      case PaintOpType::kAnnotate: {
        const auto& op = static_cast<const AnnotateOp&>(base_op);
        str << "type=" << ToString(op.annotation_type)
            << ", rect=" << ToString(op.rect) << ", data=" << ToString(op.data);
        break;
      }
      case PaintOpType::kClipPath: {
        const auto& op = static_cast<const ClipPathOp&>(base_op);
        str << "path=" << ToString(op.path) << ", op=" << ToString(op.op)
            << ", antialias=" << op.antialias
            << ", use_cache=" << (op.use_cache == UsePaintCache::kEnabled);
        break;
      }
      case PaintOpType::kClipRect: {
        const auto& op = static_cast<const ClipRectOp&>(base_op);
        str << "rect=" << ToString(op.rect) << ", op=" << ToString(op.op)
            << ", antialias=" << op.antialias;
        break;
      }
      case PaintOpType::kClipRRect: {
        const auto& op = static_cast<const ClipRRectOp&>(base_op);
        str << "rrect=" << ToString(op.rrect) << ", op=" << ToString(op.op)
            << ", antialias=" << op.antialias;
        break;
      }
      case PaintOpType::kConcat: {
        const auto& op = static_cast<const ConcatOp&>(base_op);
        str << "matrix=" << ToString(op.matrix);
        break;
      }
      case PaintOpType::kCustomData: {
        const auto& op = static_cast<const CustomDataOp&>(base_op);
        str << "id=" << ToString(op.id);
        break;
      }
      case PaintOpType::kDrawColor: {
        const auto& op = static_cast<const DrawColorOp&>(base_op);
        str << "color=" << ToString(op.color) << ", mode=" << ToString(op.mode);
        break;
      }
      case PaintOpType::kDrawDRRect: {
        const auto& op = static_cast<const DrawDRRectOp&>(base_op);
        str << "outer=" << ToString(op.outer)
            << ", inner=" << ToString(op.inner)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawImage: {
        const auto& op = static_cast<const DrawImageOp&>(base_op);
        str << "image=" << ToString(op.image) << ", left=" << ToString(op.left)
            << ", top=" << ToString(op.top) << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawImageRect: {
        const auto& op = static_cast<const DrawImageRectOp&>(base_op);
        str << "image=" << ToString(op.image) << ", src=" << ToString(op.src)
            << ", dst=" << ToString(op.dst)
            << ", constraint=" << ToString(op.constraint)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawIRect: {
        const auto& op = static_cast<const DrawIRectOp&>(base_op);
        str << "rect=" << ToString(op.rect) << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawLine: {
        const auto& op = static_cast<const DrawLineOp&>(base_op);
        str << "x0=" << ToString(op.x0) << ", y0=" << ToString(op.y0)
            << ", x1=" << ToString(op.x1) << ", y1=" << ToString(op.y1)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawLineLite: {
        const auto& op = static_cast<const DrawLineLiteOp&>(base_op);
        str << "x0=" << ToString(op.x0) << ", y0=" << ToString(op.y0)
            << ", x1=" << ToString(op.x1) << ", y1=" << ToString(op.y1)
            << ", flags=" << ToString(op.core_paint_flags);
        break;
      }
      case PaintOpType::kDrawArc: {
        const auto& op = static_cast<const DrawArcOp&>(base_op);
        str << "oval=" << ToString(op.oval)
            << ", start_angle=" << ToString(op.start_angle_degrees)
            << ", sweep_angle=" << ToString(op.sweep_angle_degrees)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawArcLite: {
        const auto& op = static_cast<const DrawArcLiteOp&>(base_op);
        str << "oval=" << ToString(op.oval)
            << ", start_angle=" << ToString(op.start_angle_degrees)
            << ", sweep_angle=" << ToString(op.sweep_angle_degrees)
            << ", flags=" << ToString(op.core_paint_flags);
        break;
      }
      case PaintOpType::kDrawOval: {
        const auto& op = static_cast<const DrawOvalOp&>(base_op);
        str << "oval=" << ToString(op.oval) << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawPath: {
        const auto& op = static_cast<const DrawPathOp&>(base_op);
        str << "path=" << ToString(op.path) << ", flags=" << ToString(op.flags)
            << ", use_cache=" << (op.use_cache == UsePaintCache::kEnabled);
        break;
      }
      case PaintOpType::kDrawRecord: {
        const auto& op = static_cast<const DrawRecordOp&>(base_op);
        str << "record=" << ToString(op.record);
        break;
      }
      case PaintOpType::kDrawRect: {
        const auto& op = static_cast<const DrawRectOp&>(base_op);
        str << "rect=" << ToString(op.rect) << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawRRect: {
        const auto& op = static_cast<const DrawRRectOp&>(base_op);
        str << "rrect=" << ToString(op.rrect)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawScrollingContents: {
        str << "scroll_element_id="
            << static_cast<const DrawScrollingContentsOp&>(base_op)
                   .scroll_element_id;
        break;
      }
      case PaintOpType::kDrawSkottie: {
        const auto& op = static_cast<const DrawSkottieOp&>(base_op);
        str << "skottie=" << ToString(op.skottie)
            << ", dst=" << ToString(op.dst) << ", t=" << op.t;
        break;
      }
      case PaintOpType::kDrawTextBlob: {
        const auto& op = static_cast<const DrawTextBlobOp&>(base_op);
        str << "blob=" << ToString(op.blob) << ", x=" << ToString(op.x)
            << ", y=" << ToString(op.y) << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawSlug: {
        const auto& op = static_cast<const DrawSlugOp&>(base_op);
        str << "flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kDrawVertices: {
        const auto& op = static_cast<const DrawVerticesOp&>(base_op);
        str << "flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kNoop:
        break;
      case PaintOpType::kRestore:
        break;
      case PaintOpType::kRotate: {
        const auto& op = static_cast<const RotateOp&>(base_op);
        str << "degrees=" << ToString(op.degrees);
        break;
      }
      case PaintOpType::kSave:
        break;
      case PaintOpType::kSaveLayer: {
        const auto& op = static_cast<const SaveLayerOp&>(base_op);
        str << "bounds=" << ToString(op.bounds)
            << ", flags=" << ToString(op.flags);
        break;
      }
      case PaintOpType::kSaveLayerAlpha: {
        const auto& op = static_cast<const SaveLayerAlphaOp&>(base_op);
        str << "bounds=" << ToString(op.bounds)
            << ", alpha=" << ToString(op.alpha);
        break;
      }
      case PaintOpType::kSaveLayerFilters: {
        const auto& op = static_cast<const SaveLayerFiltersOp&>(base_op);
        str << "flags=" << ToString(op.flags)
            << ", filters=" << ToString(op.filters);
        break;
      }
      case PaintOpType::kScale: {
        const auto& op = static_cast<const ScaleOp&>(base_op);
        str << "sx=" << ToString(op.sx) << ", sy=" << ToString(op.sy);
        break;
      }
      case PaintOpType::kSetMatrix: {
        const auto& op = static_cast<const SetMatrixOp&>(base_op);
        str << "matrix=" << ToString(op.matrix);
        break;
      }
      case PaintOpType::kTranslate: {
        const auto& op = static_cast<const TranslateOp&>(base_op);
        str << "dx=" << ToString(op.dx) << ", dy=" << ToString(op.dy);
        break;
      }
      case PaintOpType::kSetNodeId: {
        const auto& op = static_cast<const SetNodeIdOp&>(base_op);
        str << "id=" << op.node_id;
        break;
      }
    }
    str << ")";
    return str.str();
  }

  static std::string ToString(uint8_t value) {
    return base::StringPrintf("%d", value);
  }

  static std::string ToString(const SkScalar& scalar) {
    return base::StringPrintf("%.3f", scalar);
  }

  static std::string ToString(const SkPoint& point) {
    return base::StringPrintf("[%.3f,%.3f]", point.fX, point.fY);
  }

  static std::string ToString(const SkRect& rect) {
    return base::StringPrintf("[%.3f,%.3f %.3fx%.3f]", rect.x(), rect.y(),
                              rect.width(), rect.height());
  }

  static std::string ToString(const SkIRect& rect) {
    return base::StringPrintf("[%d,%d %dx%d]", rect.x(), rect.y(), rect.width(),
                              rect.height());
  }

  static std::string ToString(const SkRRect& rect) {
    return base::StringPrintf("[bounded by %.3f,%.3f %.3fx%.3f]",
                              rect.rect().x(), rect.rect().y(),
                              rect.rect().width(), rect.rect().height());
  }

  static std::string ToString(const SkRegion& rect) {
    return ToString(rect.getBounds());
  }

  static std::string ToString(const SkM44& matrix) {
    return base::StringPrintf(
        "[%8.4f %8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f "
        "%8.4f][%8.4f %8.4f %8.4f %8.4f]]",
        matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(0, 3),
        matrix.rc(1, 0), matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(1, 3),
        matrix.rc(2, 0), matrix.rc(2, 1), matrix.rc(2, 2), matrix.rc(2, 3),
        matrix.rc(3, 0), matrix.rc(3, 1), matrix.rc(3, 2), matrix.rc(3, 3));
  }

  static std::string ToString(const SkMatrix& matrix) {
    return base::StringPrintf(
        "[%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f]]",
        matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(1, 0),
        matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(2, 0), matrix.rc(2, 1),
        matrix.rc(2, 2));
  }

  static std::string ToString(const SkColor& color) {
    return base::StringPrintf("rgba(%d, %d, %d, %d)", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color),
                              SkColorGetA(color));
  }

  static std::string ToString(const SkColor4f& color) {
    return base::StringPrintf("rgba(%f, %f, %f, %f)", color.fR, color.fG,
                              color.fB, color.fA);
  }

  static std::string ToString(const SkISize& size) {
    return base::StringPrintf("SkISize(%d, %d)", size.width(), size.height());
  }

  static std::string ToString(const SkIPoint& point) {
    return base::StringPrintf("SkIPoint(%d, %d)", point.x(), point.y());
  }

  static std::string ToString(const SkPoint3& point) {
    return base::StringPrintf(
        "SkPoint3(%s, %s, %s)", ToString(point.x()).c_str(),
        ToString(point.y()).c_str(), ToString(point.z()).c_str());
  }

  static std::string ToString(SkColorChannel channel) {
    switch (channel) {
      case SkColorChannel::kR:
        return "kR";
      case SkColorChannel::kG:
        return "kG";
      case SkColorChannel::kB:
        return "kB";
      case SkColorChannel::kA:
        return "kA";
    }
    NOTREACHED();
  }

  static std::string ToString(const SkBlendMode& mode) {
    switch (mode) {
      default:
        break;
      case SkBlendMode::kClear:
        return "kClear";
      case SkBlendMode::kSrc:
        return "kSrc";
      case SkBlendMode::kDst:
        return "kDst";
      case SkBlendMode::kSrcOver:
        return "kSrcOver";
      case SkBlendMode::kDstOver:
        return "kDstOver";
      case SkBlendMode::kSrcIn:
        return "kSrcIn";
      case SkBlendMode::kDstIn:
        return "kDstIn";
      case SkBlendMode::kSrcOut:
        return "kSrcOut";
      case SkBlendMode::kDstOut:
        return "kDstOut";
      case SkBlendMode::kSrcATop:
        return "kSrcATop";
      case SkBlendMode::kDstATop:
        return "kDstATop";
      case SkBlendMode::kXor:
        return "kXor";
      case SkBlendMode::kPlus:
        return "kPlus";
      case SkBlendMode::kModulate:
        return "kModulate";
      case SkBlendMode::kScreen:
        return "kScreen";
      case SkBlendMode::kOverlay:
        return "kOverlay";
      case SkBlendMode::kDarken:
        return "kDarken";
      case SkBlendMode::kLighten:
        return "kLighten";
      case SkBlendMode::kColorDodge:
        return "kColorDodge";
      case SkBlendMode::kColorBurn:
        return "kColorBurn";
      case SkBlendMode::kHardLight:
        return "kHardLight";
      case SkBlendMode::kSoftLight:
        return "kSoftLight";
      case SkBlendMode::kDifference:
        return "kDifference";
      case SkBlendMode::kExclusion:
        return "kExclusion";
      case SkBlendMode::kMultiply:
        return "kMultiply";
      case SkBlendMode::kHue:
        return "kHue";
      case SkBlendMode::kSaturation:
        return "kSaturation";
      case SkBlendMode::kColor:
        return "kColor";
      case SkBlendMode::kLuminosity:
        return "kLuminosity";
    }
    return "<unknown SkBlendMode>";
  }

  static std::string ToString(const SkClipOp& op) {
    switch (op) {
      default:
        break;
      case SkClipOp::kDifference:
        return "kDifference";
      case SkClipOp::kIntersect:
        return "kIntersect";
    }
    return "<unknown SkClipOp>";
  }

  static std::string ToString(const SkData& data) { return "<SkData>"; }

  static std::string ToString(const ThreadsafePath& path) {
    return ToString(static_cast<const SkPath&>(path));
  }

  static std::string ToString(const SkPath& path) {
    // TODO(vmpstr): SkPath has a dump function which we can use here?
    return "<SkPath>";
  }

  static std::string ToString(PaintFlags::FilterQuality quality) {
    switch (quality) {
      case PaintFlags::FilterQuality::kNone:
        return "kNone_SkFilterQuality";
      case PaintFlags::FilterQuality::kLow:
        return "kLow_SkFilterQuality";
      case PaintFlags::FilterQuality::kMedium:
        return "kMedium_SkFilterQuality";
      case PaintFlags::FilterQuality::kHigh:
        return "kHigh_SkFilterQuality";
    }
    return "<unknown FilterQuality>";
  }

  static std::string ToString(PaintFlags::Cap cap) {
    switch (cap) {
      case PaintFlags::kButt_Cap:
        return "kButt_Cap";
      case PaintFlags::kRound_Cap:
        return "kRound_Cap";
      case PaintFlags::kSquare_Cap:
        return "kSquare_Cap";
    }
    return "<unknown PaintFlags::Cap>";
  }

  static std::string ToString(PaintFlags::Join join) {
    switch (join) {
      case PaintFlags::kMiter_Join:
        return "kMiter_Join";
      case PaintFlags::kRound_Join:
        return "kRound_Join";
      case PaintFlags::kBevel_Join:
        return "kBevel_Join";
    }
    return "<unknown PaintFlags::Join>";
  }

  static std::string ToString(TurbulencePaintFilter::TurbulenceType type) {
    switch (type) {
      case TurbulencePaintFilter::TurbulenceType::kTurbulence:
        return "kTurbulence";
      case TurbulencePaintFilter::TurbulenceType::kFractalNoise:
        return "kFractalNoise";
    }
    return "<unknown TurbulencePaintFilter::TurbulenceType>";
  }

  static std::string ToString(MorphologyPaintFilter::MorphType type) {
    switch (type) {
      case MorphologyPaintFilter::MorphType::kDilate:
        return "kDilate";
      case MorphologyPaintFilter::MorphType::kErode:
        return "kErode";
    }
    return "<unknown MorphologyPaintFilter::MorphType>";
  }

  static std::string ToString(PaintFilter::LightingType type) {
    switch (type) {
      case PaintFilter::LightingType::kDiffuse:
        return "kDiffuse";
      case PaintFilter::LightingType::kSpecular:
        return "kSpecular";
    }
    return "<unknown PaintFilter::LightingType>";
  }

  static std::string ToString(DropShadowPaintFilter::ShadowMode shadow_mode) {
    switch (shadow_mode) {
      case DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground:
        return "kDrawShadowAndForeground";
      case DropShadowPaintFilter::ShadowMode::kDrawShadowOnly:
        return "kDrawShadowOnly";
    }
    return "<unknown DropShadowPaintFilter::ShadowMode>";
  }

  static std::string ToString(const SkImageFilters::Dither dither) {
    switch (dither) {
      case SkImageFilters::Dither::kNo:
        return "kNo";
      case SkImageFilters::Dither::kYes:
        return "kYes";
    }
    return "<unknown SkImageFilters::Dither>";
  }

  static std::string ToString(const ColorFilter& filter) {
    return "ColorFilter";
  }

  static std::string ToString(const PathEffect& effect) { return "PathEffect"; }

  static std::string ToString(const DrawLooper& looper) { return "DrawLooper"; }

  static std::string ToString(PaintCanvas::AnnotationType type) {
    switch (type) {
      default:
        break;
      case PaintCanvas::AnnotationType::kUrl:
        return "URL";
      case PaintCanvas::AnnotationType::kNameDestination:
        return "NAMED_DESTINATION";
      case PaintCanvas::AnnotationType::kLinkToDestination:
        return "LINK_TO_DESTINATION";
    }
    return "<unknown AnnotationType>";
  }

  static std::string ToString(const SkCanvas::SrcRectConstraint& constraint) {
    switch (constraint) {
      default:
        break;
      case SkCanvas::kStrict_SrcRectConstraint:
        return "kStrict_SrcRectConstraint";
      case SkCanvas::kFast_SrcRectConstraint:
        return "kFast_SrcRectConstraint";
    }
    return "<unknown SrcRectConstraint>";
  }

  static std::string ToString(PaintShader::ScalingBehavior behavior) {
    switch (behavior) {
      case PaintShader::ScalingBehavior::kRasterAtScale:
        return "kRasterAtScale";
      case PaintShader::ScalingBehavior::kFixedScale:
        return "kFixedScale";
    }
    return "<unknown ScalingBehavior>";
  }

  static std::string ToString(PaintShader::Type type) {
    switch (type) {
      case PaintShader::Type::kEmpty:
        return "kEmpty";
      case PaintShader::Type::kColor:
        return "kColor";
      case PaintShader::Type::kLinearGradient:
        return "kLinearGradient";
      case PaintShader::Type::kRadialGradient:
        return "kRadialGradient";
      case PaintShader::Type::kTwoPointConicalGradient:
        return "kTwoPointConicalGradient";
      case PaintShader::Type::kSweepGradient:
        return "kSweepGradient";
      case PaintShader::Type::kImage:
        return "kImage";
      case PaintShader::Type::kPaintRecord:
        return "kPaintRecord";
      case PaintShader::Type::kShaderCount:
        return "kShaderCount";
    }
    return "<unknown PaintShader::Type>";
  }

  static std::string ToString(const PaintImage& image) {
    return "<paint image>";
  }

  static std::string ToString(const SkottieWrapper& skottie) {
    std::ostringstream str;
    str << "<skottie [";
    str << "duration=" << skottie.duration() << " seconds";
    str << ", width=" << ToString(skottie.size().width());
    str << ", height=" << ToString(skottie.size().height());
    str << "]";
    return str.str();
  }

  static std::string ToString(const PaintRecord& record) {
    std::ostringstream str;
    str << "<PaintRecord>[";
    bool is_first = true;
    for (const PaintOp& op : record) {
      if (!is_first) {
        str << ", ";
      }
      str << ToString(op);
      is_first = false;
    }
    str << "]";
    return str.str();
  }

  static std::string ToString(const SkTextBlob& blob) {
    return "<sk text blob>";
  }

  static std::string ToString(SkTileMode tile_mode) {
    switch (tile_mode) {
      case SkTileMode::kClamp:
        return "kClamp";
      case SkTileMode::kRepeat:
        return "kRepeat";
      case SkTileMode::kMirror:
        return "kMirror";
      case SkTileMode::kDecal:
        return "kDecal";
    }
    return "<unknown SkTileMode>";
  }

  static std::string ToString(const gfx::SizeF& size) {
    return base::StringPrintf("[%s]", size.ToString().c_str());
  }

  static std::string ToString(const PaintShader& shader) {
    std::ostringstream str;
    str << "[type=" << ToString(shader.shader_type());
    str << ", flags=" << shader.flags_;
    str << ", end_radius=" << shader.end_radius_;
    str << ", start_radius=" << shader.start_radius_;
    str << ", tx=" << static_cast<unsigned>(shader.tx_);
    str << ", ty=" << static_cast<unsigned>(shader.ty_);
    str << ", fallback_color=" << ToString(shader.fallback_color_);
    str << ", scaling_behavior=" << ToString(shader.scaling_behavior_);
    str << ", local_matrix=" << ToString(shader.local_matrix_);
    str << ", center=" << ToString(shader.center_);
    str << ", tile=" << ToString(shader.tile_);
    str << ", start_point=" << ToString(shader.start_point_);
    str << ", end_point=" << ToString(shader.end_point_);
    str << ", start_degrees=" << shader.start_degrees_;
    str << ", end_degrees=" << shader.end_degrees_;
    if (shader.shader_type() == PaintShader::Type::kImage) {
      str << ", image=" << ToString(shader.image_);
    } else {
      str << ", image=(nil)";
    }
    str << ", record=" << ToString(shader.record_);
    str << ", id=" << shader.id_;
    str << ", tile_scale=" << ToString(shader.tile_scale_);
    if (shader.colors_.size() > 0) {
      str << ", colors=[" << ToString(shader.colors_[0]);
      for (size_t i = 1; i < shader.colors_.size(); ++i) {
        str << ", " << ToString(shader.colors_[i]);
      }
      str << "]";
    } else {
      str << ", colors=(nil)";
    }
    if (shader.positions_.size() > 0) {
      str << ", positions=[" << shader.positions_[0];
      for (size_t i = 1; i < shader.positions_.size(); ++i) {
        str << ", " << shader.positions_[i];
      }
      str << "]";
    } else {
      str << ", positions=(nil)";
    }
    str << "]";

    return str.str();
  }

  static std::string ToString(const PaintFilter& base_filter) {
    std::ostringstream str;
    str << std::boolalpha;
    switch (base_filter.type()) {
      case PaintFilter::Type::kNullFilter:
        str << "NullFilter()";
        break;
      case PaintFilter::Type::kColorFilter: {
        const auto& filter =
            static_cast<const ColorFilterPaintFilter&>(base_filter);
        str << "ColorFilterPaintFilter("
            << "color_filter=" << ToString(filter.color_filter())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kBlur: {
        const auto& filter = static_cast<const BlurPaintFilter&>(base_filter);

        str << "BlurPaintFilter("
            << "sigma_x=" << ToString(filter.sigma_x())
            << ", sigma_y=" << ToString(filter.sigma_y())
            << ", tile_mode=" << ToString(filter.tile_mode())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kDropShadow: {
        const auto& filter =
            static_cast<const DropShadowPaintFilter&>(base_filter);
        str << "DropShadowPaintFilter("
            << "dx=" << ToString(filter.dx())
            << ", dy=" << ToString(filter.dy())
            << ", sigma_x=" << ToString(filter.sigma_x())
            << ", sigma_y=" << ToString(filter.sigma_y())
            << ", color=" << ToString(filter.color())
            << ", shadow_mode=" << ToString(filter.shadow_mode())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kMagnifier: {
        const auto& filter =
            static_cast<const MagnifierPaintFilter&>(base_filter);
        str << "MagnifierPaintFilter("
            << "lens_bounds=" << ToString(filter.lens_bounds())
            << ", zoom_amount=" << ToString(filter.zoom_amount())
            << ", inset=" << ToString(filter.inset())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kCompose: {
        const auto& filter =
            static_cast<const ComposePaintFilter&>(base_filter);
        str << "ComposePaintFilter("
            << "outer=" << ToString(filter.outer())
            << ", inner=" << ToString(filter.inner())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kAlphaThreshold: {
        const auto& filter =
            static_cast<const AlphaThresholdPaintFilter&>(base_filter);
        str << "AlphaThresholdPaintFilter("
            << "region=" << ToString(filter.region())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kXfermode: {
        const auto& filter =
            static_cast<const XfermodePaintFilter&>(base_filter);
        str << "XfermodePaintFilter("
            << "blend_mode=" << ToString(filter.blend_mode())
            << ", background=" << ToString(filter.background())
            << ", foreground=" << ToString(filter.foreground())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kArithmetic: {
        const auto& filter =
            static_cast<const ArithmeticPaintFilter&>(base_filter);
        str << "ArithmeticPaintFilter("
            << "k1=" << filter.k1() << ", k2=" << filter.k2()
            << ", k3=" << filter.k3() << ", k4=" << filter.k4()
            << ", enfore_pm_color=" << filter.enforce_pm_color()
            << ", background=" << ToString(filter.background())
            << ", foreground=" << ToString(filter.foreground())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kMatrixConvolution: {
        const auto& filter =
            static_cast<const MatrixConvolutionPaintFilter&>(base_filter);
        const SkISize& kernel_size = filter.kernel_size();
        str << "MatrixConvolutionPaintFilter("
            << "kernel_size=" << ToString(kernel_size) << ", kernel=[";
        for (int32_t i = 0; i < kernel_size.width() * kernel_size.height();
             ++i) {
          str << (i != 0 ? ", " : "") << ToString(filter.kernel_at(i));
        }
        str << "], gain=" << ToString(filter.gain())
            << ", bias=" << ToString(filter.bias())
            << ", kernel_offset=" << ToString(filter.kernel_offset())
            << ", tile_mode=" << ToString(filter.tile_mode())
            << ", convolve_alpha=" << filter.convolve_alpha()
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kDisplacementMapEffect: {
        const auto& filter =
            static_cast<const DisplacementMapEffectPaintFilter&>(base_filter);
        str << "DisplacementMapEffectPaintFilter("
            << "channel_x=" << ToString(filter.channel_x())
            << ", channel_y=" << ToString(filter.channel_y())
            << ", scale=" << ToString(filter.scale())
            << ", displacement=" << ToString(filter.displacement())
            << ", color=" << ToString(filter.color())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kImage: {
        const auto& filter = static_cast<const ImagePaintFilter&>(base_filter);
        str << "ImagePaintFilter("
            << "image=" << ToString(filter.image())
            << ", src_rect=" << ToString(filter.src_rect())
            << ", dst_rect=" << ToString(filter.dst_rect())
            << ", filter_quality=" << ToString(filter.filter_quality())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kPaintRecord: {
        const auto& filter = static_cast<const RecordPaintFilter&>(base_filter);
        str << "RecordPaintFilter("
            << "record=" << ToString(filter.record())
            << ", record_bounds=" << ToString(filter.record_bounds())
            << ", raster_scale=" << ToString(filter.raster_scale())
            << ", scaling_behavior=" << ToString(filter.scaling_behavior())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kMerge: {
        const auto& filter = static_cast<const MergePaintFilter&>(base_filter);
        str << "MergePaintFilter("
            << "input_count=" << filter.input_count() << ", input=[";
        for (size_t i = 0; i < filter.input_count(); ++i) {
          str << (i != 0 ? ", " : "") << ToString(filter.input_at(i));
        }
        str << "], crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kMorphology: {
        const auto& filter =
            static_cast<const MorphologyPaintFilter&>(base_filter);
        str << "MorphologyPaintFilter("
            << "morph_type=" << ToString(filter.morph_type())
            << ", radius_x=" << filter.radius_x()
            << ", radius_y=" << filter.radius_y()
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kOffset: {
        const auto& filter = static_cast<const OffsetPaintFilter&>(base_filter);
        str << "OffsetPaintFilter("
            << "dx=" << filter.dx() << ", dy=" << filter.dy()
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kTile: {
        const auto& filter = static_cast<const TilePaintFilter&>(base_filter);
        str << "TilePaintFilter("
            << "src=" << ToString(filter.src())
            << ", dst=" << ToString(filter.dst())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kTurbulence: {
        const auto& filter =
            static_cast<const TurbulencePaintFilter&>(base_filter);
        str << "TurbulencePaintFilter("
            << "turbulence_type=" << ToString(filter.turbulence_type())
            << ", base_frequency_x=" << ToString(filter.base_frequency_x())
            << ", base_frequency_y=" << ToString(filter.base_frequency_y())
            << ", num_octaves=" << filter.num_octaves()
            << ", seed=" << ToString(filter.seed())
            << ", tile_size=" << ToString(filter.tile_size())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kShader: {
        const auto& filter = static_cast<const ShaderPaintFilter&>(base_filter);
        str << "ShaderPaintFilter("
            << "shader=" << ToString(filter.shader())
            << ", alpha=" << ToString(filter.alpha())
            << ", filter_quality=" << ToString(filter.filter_quality())
            << ", dither=" << ToString(filter.dither())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kMatrix: {
        const auto& filter = static_cast<const MatrixPaintFilter&>(base_filter);
        str << "MatrixPaintFilter("
            << "matrix=" << ToString(filter.matrix())
            << ", filter_quality=" << ToString(filter.filter_quality())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kLightingDistant: {
        const auto& filter =
            static_cast<const LightingDistantPaintFilter&>(base_filter);
        str << "LightingDistantPaintFilter("
            << "lighting_type=" << ToString(filter.lighting_type())
            << ", direction=" << ToString(filter.direction())
            << ", light_color=" << ToString(filter.light_color())
            << ", surface_scale=" << ToString(filter.surface_scale())
            << ", kconstant=" << ToString(filter.kconstant())
            << ", shininess=" << ToString(filter.shininess())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kLightingPoint: {
        const auto& filter =
            static_cast<const LightingPointPaintFilter&>(base_filter);
        str << "LightingPointPaintFilter("
            << "lighting_type=" << ToString(filter.lighting_type())
            << ", location=" << ToString(filter.location())
            << ", light_color=" << ToString(filter.light_color())
            << ", surface_scale=" << ToString(filter.surface_scale())
            << ", kconstant=" << ToString(filter.kconstant())
            << ", shininess=" << ToString(filter.shininess())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
        break;
      }
      case PaintFilter::Type::kLightingSpot: {
        const auto& filter =
            static_cast<const LightingSpotPaintFilter&>(base_filter);
        str << "LightingSpotPaintFilter("
            << "lighting_type=" << ToString(filter.lighting_type())
            << ", location=" << ToString(filter.location())
            << ", target=" << ToString(filter.target())
            << ", specular_exponent=" << ToString(filter.specular_exponent())
            << ", cutoff_angle=" << ToString(filter.cutoff_angle())
            << ", light_color=" << ToString(filter.light_color())
            << ", surface_scale=" << ToString(filter.surface_scale())
            << ", kconstant=" << ToString(filter.kconstant())
            << ", shininess=" << ToString(filter.shininess())
            << ", input=" << ToString(filter.input())
            << ", crop_rect=" << ToString(filter.GetCropRect()) << ")";
      }
    }
    return str.str();
  }

  static std::string ToString(const PaintFlags& flags) {
    std::ostringstream str;
    str << std::boolalpha;
    str << "[color=" << ToString(flags.getColor());
    str << ", blendMode=" << ToString(flags.getBlendMode());
    str << ", isAntiAlias=" << flags.isAntiAlias();
    str << ", isDither=" << flags.isDither();
    str << ", filterQuality=" << ToString(flags.getFilterQuality());
    str << ", strokeWidth=" << ToString(flags.getStrokeWidth());
    str << ", strokeMiter=" << ToString(flags.getStrokeMiter());
    str << ", strokeCap=" << ToString(flags.getStrokeCap());
    str << ", strokeJoin=" << ToString(flags.getStrokeJoin());
    str << ", colorFilter=" << ToString(flags.getColorFilter());
    str << ", shader=" << ToString(flags.getShader());
    str << ", hasShader=" << flags.HasShader();
    str << ", shaderIsOpaque=" << (flags.HasShader() && flags.ShaderIsOpaque());
    str << ", pathEffect=" << ToString(flags.getPathEffect());
    str << ", imageFilter=" << ToString(flags.getImageFilter());
    str << ", drawLooper=" << ToString(flags.getLooper());
    str << ", supportsFoldingAlpha=" << flags.SupportsFoldingAlpha();
    str << ", isValid=" << flags.IsValid();
    str << ", hasDiscardableImages=" << flags.HasDiscardableImages();
    str << "]";
    return str.str();
  }

  static std::string ToString(const CorePaintFlags& flags) {
    PaintFlags paint_flags(flags);
    return ToString(paint_flags);
  }
};

}  // namespace cc

#endif  // CC_TEST_PAINT_OP_HELPER_H_
